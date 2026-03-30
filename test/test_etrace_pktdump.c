// SPDX-License-Identifier: BSD-2-Clause
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
*/

#include <errno.h>
#include <sys/stat.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <rv_etrace_encap.h>
#include <rv_etrace_itrace.h>
#include <rv_etrace_display.h>

#define debug
#define PERF_MAGIC 0x32454c4946524550 /* PERFILE2 */
#define PERF_DATA_SEC_OFF 0x28

struct __attribute__((packed)) perf_event_header {
	uint32_t type;
	uint16_t misc;
	uint16_t size;
};

struct __attribute__((packed)) perf_file_section {
	uint64_t offset;
	uint64_t size;
};

struct __attribute__((packed)) perf_record_auxtrace {
	struct perf_event_header header;
	uint64_t			 size;
	uint64_t			 offset;
	uint64_t			 reference;
	uint32_t			 idx;
	uint32_t			 tid;
	uint32_t			 cpu;
	uint32_t			 reserved__; /* For alignment */
};

static int parse_params_csv(const char *filename, bool show_params)
{
#define MAX_LINES		512
	char itrace_params[MAX_LINES][MAX_LINE_LENGTH];
	int value, count = 0;
	FILE *file;
	size_t len;

	file = fopen(filename, "r");
	if (!file) {
		perror("Error opening file");
		return -ENOENT;
	}

	while (fgets(itrace_params[count], MAX_LINE_LENGTH, file) != NULL && count < MAX_LINES) {
		len = strlen(itrace_params[count]);
		if (len > 0 && itrace_params[count][len - 1] == '\n') {
			itrace_params[count][len - 1] = '\0';
		}
		count++;
	}

	fclose(file);
	return rv_etrace_parse_params(itrace_params, show_params);
}

static void display_help(const char *exename)
{
	printf("Usage: %s -p <params_csv_file_path> -d <packet_data_file_path> [<options>]\n",
		exename);
	printf("Options:\n");
	printf("  -v\t\tShow parameters\n");
	printf("  -c\t\tCpu (for perf data)\n");
	printf("  -h\t\tDisplay help\n");
}

static int read_bytes(FILE *file, off_t from, size_t bytes, void *buf)
{
	size_t bytes_read;
	int ret;

	errno = 0;
	ret = fseek(file, from, SEEK_SET);
	if (ret) {
		printf("%s failed. Error %d(%s)\n", __func__, errno, strerror(errno));
		return ret;
	}

	bytes_read = fread(buf, 1, bytes, file);
	ret = bytes_read - bytes;
	if (ret < 0) {
		if (feof(file))
			printf("%s failed. Error EOF\n",__func__);

		if (ferror(file))
			printf("%s failed. Error %d(%s)\n", __func__, errno, strerror(errno));
	}

	return ret;
}

static int find_aux_header(FILE *file, size_t *size, off_t *offset, int cpu)
{
	struct perf_file_section data_sec;
	struct perf_record_auxtrace aux;
	struct perf_event_header evt;
	off_t evt_off;
	int ret;

	ret = read_bytes(file, PERF_DATA_SEC_OFF, sizeof(data_sec), &data_sec);
	if (ret)
		return ret;

	debug("%s:%d data_sec.offset %lx, data_sec.size %lx\n",
		__func__, __LINE__, data_sec.offset, data_sec.size);

	evt_off = data_sec.offset;
	do {
		ret = read_bytes(file, evt_off, sizeof(evt), &evt);
		if (ret)
			return ret;

		if (evt.type == 71) {
			ret = read_bytes(file, evt_off, sizeof(aux), &aux);
			if (ret)
				return ret;
			if (aux.cpu == cpu) {
				debug("Found auxtrace record at %lx, size %lx offset %lx\n",
						evt_off, aux.size, aux.offset);
				*size = aux.size;
				*offset = evt_off;
				return 0;
			} else
				evt_off += aux.size;
		}
		evt_off += evt.size;
	} while(evt_off < (data_sec.size + data_sec.offset));

	errno = -ENODATA;
	return -1;
}

int main(int argc, char *argv[])  
{
	const char *csv_filename = NULL, *pktdata_filename = NULL;
	bool show_params = false, is_perf_data = false;
	unsigned char *packet_stream;
	int rc = 0, opt;
	uint64_t magic;
	struct stat st;
	char *errstr;
	size_t size;
	FILE *file;
	off_t off;
	int cpu;

	while((opt = getopt(argc, argv, "p:d:c:hv")) != -1) {
		switch(opt) {
		case 'p':
			csv_filename = optarg;
			break;
		case 'd':
			pktdata_filename = optarg;
			break;
		case 'c':
			cpu = strtoul(optarg, &errstr, 10);
			if (*errstr != '\0') {
				printf("error. invalid cpu\n");
				return -EINVAL;
			}
			break;
		case 'h':
			display_help(argv[0]);
			return 0;
		case 'v':
			show_params = true;
			break;
		default:
			printf("error: unknown option: %c\n", optopt);
			display_help(argv[0]);
			return -ENOENT;
		}
	}

	if (!csv_filename) {
		printf("error: must provide parameters CSV file path\n");
		display_help(argv[0]);
		rc = -ENOENT;
		goto fail;
	}

	rc = stat(csv_filename, &st);
	if (rc) {
		printf("error: parameters CSV file %s does not exists\n", csv_filename);
		goto fail;
	}

	if (!pktdata_filename) {
		printf("error: must provide packet data file path\n");
		display_help(argv[0]);
		rc = -ENOENT;
		goto fail;
	}

	rc = stat(pktdata_filename, &st);
	if (rc) {
		printf("error: packet data file %s does not exists\n", pktdata_filename);
		goto fail;
	}

	rc = parse_params_csv(csv_filename, show_params);
	if (rc) {
		printf("error: parameters parsing error %d\n", rc);
		goto fail;
	}

	file = fopen(pktdata_filename, "rb");
	if (!file) {
		perror("Error opening file");
		rc = -ENOENT;
		goto fail;
	}

	if (fread(&magic, sizeof(magic), 1, file) != 1) {
		printf("error: failed to read file header\n");
		rc = -EIO;
		goto fail_free_stream;
	}

	if (magic == PERF_MAGIC) {
		rc = find_aux_header(file, &size, &off, cpu);
		if (rc < 0) {
			printf("error: No aux data found\n");
			goto fail_fclose;
		}

		rc = fseek(file, off, SEEK_SET);
		if (rc < 0) {
			printf("error: failed to seek at %lx\n", off);
			goto fail_fclose;
		}
	} else {
		size = st.st_size;
		rewind(file);
	}

	packet_stream = malloc(size);
	if (!packet_stream) {
		printf("error: failed to allocate %ld bytes packet stream\n", size);
		rc = -ENOMEM;
		goto fail_fclose;
	}

	if (fread(packet_stream, sizeof(*packet_stream), size, file) != size) {
		printf("error: failed to read packet stream\n");
		rc = -EIO;
		goto fail_free_stream;
	}

	rv_etrace_pktdump(packet_stream, size, 0x0);

fail_free_stream:
	free(packet_stream);
fail_fclose:
	fclose(file);
fail:
	return rc;
}
