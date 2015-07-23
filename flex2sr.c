/**
 * @file flex2sr.c
 * @brief FLEX binary to Motorola S-record converter
 * @details
 *   Usage: flex2sr infile outfile
 *   It is recommended that the output be put through srec_cat(1)
 *   or similar before further use, as this program generates records
 *   as long as those in the input file.
 * @author David Knoll <david@davidknoll.me.uk>
 * @copyright MIT License
 * @date 19/07/2015
 */
#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int record(FILE *infile, FILE *outfile);
void header(FILE *outfile, const char *str);
int main(int argc, char *argv[]);

/**
 * @fn int record(FILE *infile, FILE *outfile)
 * @brief
 *   Processes one record from the input file to the output file.
 * @details
 *   Zeroes between records are skipped over.
 *   Unrecognised record type identifiers are returned and not processed further.
 * @param infile Open file pointer to the input FLEX binary.
 * @param outfile Open file pointer to the output S-record file.
 * @return
 *   The record type processed.
 *   Most likely 0x02 or 0x16.
 *   EOF if encountered.
 *   Something else if an unrecognised record type.
 */
int record(FILE *infile, FILE *outfile)
{
  int rectype, nbytes, c;
  unsigned int chksum, loadaddr;

  // Skip over zeroes between records (files may have trailing zeroes).
  // Return now if EOF or unrecognised record type.
  do { rectype = fgetc(infile); } while (rectype == 0x00);
  if (rectype != 0x02 && rectype != 0x16) return rectype;

  // Retrieve record's load address and begin the record's checksum with it.
  c = fgetc(infile);
  chksum = c;
  loadaddr = c << 8;
  c = fgetc(infile);
  chksum += c;
  loadaddr |= c;

  switch (rectype) {
    case 0x02: // Binary data
      // S-record type, count and address
      nbytes = fgetc(infile);
      chksum += nbytes + 3;
      fprintf(outfile, "S1%02X%04X", nbytes + 3, loadaddr);
      // S-record data
      while (nbytes--) {
        c = fgetc(infile);
        chksum += c;
        fprintf(outfile, "%02X", c);
      }
      break;

    case 0x16: // Transfer address
      chksum += 3;
      fprintf(outfile, "S903%04X", loadaddr);
      break;
  }

  // S-record checksum, end of record
  fprintf(outfile, "%02X\n", ~chksum & 0xFF);
  return rectype;
}

/**
 * @fn void header(FILE *outfile, const char *str)
 * @brief Outputs a header (S0) record
 * @param outfile Output file pointer
 * @param str String to put in the header
 */
void header(FILE *outfile, const char *str)
{
  int count = strlen(str) + 3;
  unsigned int chksum = count;

  fprintf(outfile, "S0%02X0000", count);
  while (*str) {
    fprintf(outfile, "%02X", *str);
    chksum += *str++;
  }
  fprintf(outfile, "%02X\n", ~chksum & 0xFF);
}

/**
 * @fn int main(int argc, char *argv[])
 * @brief Main function
 * @details Open/close files and go through records until EOF or error
 * @param argc Command-line argument count
 * @param argv Command-line arguments
 * @return Zero on success, non-zero on error
 */
int main(int argc, char *argv[])
{
  FILE *infile, *outfile;
  int rectype = 0, addrrecs = 0, datarecs = 0;

  if (argc != 3) {
    fprintf(stderr, "\
FLEX binary to Motorola S-record converter\n\
Usage: %s infile outfile\n\
It is recommended that the output be put through srec_cat(1)\n\
or similar before further use, as this program generates records\n\
as long as those in the input file.\n\
", argv[0]);
    return EXIT_FAILURE;
  }

  // Open files for input and output
  infile = fopen(argv[1], "rb");
  outfile = fopen(argv[2], "wt");
  if (infile == NULL) {
    fprintf(stderr, "Error opening file %s for input.\n", argv[1]);
  } else if (outfile == NULL) {
    fprintf(stderr, "Error opening file %s for output.\n", argv[2]);

  } else {
    // Output header record containing input filename
    header(outfile, basename(argv[1]));

    // Loop processing records until EOF or error
    do {
      rectype = record(infile, outfile);
      if (rectype == 0x02) datarecs++;
      if (rectype == 0x16) addrrecs++;
    } while (rectype == 0x02 || rectype == 0x16);

    if (rectype == EOF) {
      // Output data record count
      fprintf(outfile, "S503%04X%02X\n", datarecs, ~(0x03 + ((datarecs >> 8) & 0xFF) + (datarecs & 0xFF)) & 0xFF);
      // Output null start address, if no start address record yet
      if (addrrecs == 0) fprintf(outfile, "S9030000FC\n");

    } else {
      fprintf(stderr, "Unrecognised record type %02X at offset %04X in input file.\n", rectype, (int) ftell(infile) - 1);
    }
  }

  if (infile != NULL) fclose(infile);
  if (outfile != NULL) fclose(outfile);
  return (rectype == EOF) ? EXIT_SUCCESS : EXIT_FAILURE;
}
