#include <stdio.h>
#include <string.h>
#include <stdlib.h>

int main(int argc, char **argv)
{
	char *filename = "parsed.txt";
	char *out = "out.txt";
	FILE *fp = fopen(filename, "r");
	FILE *fp_out = fopen(out, "w");
	const unsigned MAX_LENGTH = 256;
	char buffer[MAX_LENGTH];
	char save[MAX_LENGTH];
	char temp[MAX_LENGTH];
    	char* token;
	int i, j, k;
	char* r, *s, *qs, *xs;
	int q = 0, found;
	fpos_t pos;
	char q_event[64], event[64], flags[64], comm[64], dummy[64];
	int start, count, first, second, __start;

	if (fp == NULL) {
		printf("Error: could not open file %s", filename);
		return 1;
	}

	while (fgets(buffer, MAX_LENGTH, fp)) {
		qs = strstr(buffer, " Q ");
		xs = strstr(buffer, " X ");

		if (qs) {
			memcpy(temp, buffer, qs - buffer);

			/*
			 * Q  WS 285184 + 1536 [dd]
			 */
			sscanf(qs, " %s %s %d + %d %s", event, flags, &start, &count, comm);

			fgetpos(fp, &pos);
			while(fgets(buffer, MAX_LENGTH, fp)) {
				xs = strstr(buffer, " X ");
				if (xs) {
					sscanf(xs, " %s %s %d / %d", dummy, dummy, &first, &second);
					if (first == start) {
						count = second - first;
						break;
					}
				}
			}
			fsetpos(fp, &pos);

			sprintf(temp + (int)(qs - buffer), "%2s %3s %d + %d %s", event, flags, start, count, comm);
			fprintf(fp_out, "%s\n", temp);

			continue;
		}

		if (xs) {
			memcpy(temp, buffer, xs - buffer);

			/*
			 * X  WS 282624 / 285184 [dd]
			 */
			sscanf(xs, " %s %s %d / %d %s", dummy, flags, &first, &second, comm);
			sprintf(temp + (int)(xs - buffer), "%2s %3s %d + %d %s", "Q", flags, second, second - first, comm);
			fprintf(fp_out, "%s\n", temp);

			continue;
		}

		fprintf(fp_out, "%s", buffer);
	}	

	fclose(fp);

	return 0;
}
