/**
 * @file sr2flex.c
 * @brief Motorola S-record to FLEX binary converter
 * @details
 *   Usage: sr2flex infile outfile
 *   Output records are the same size as input records, so may not
 *   be as large as possible even where data is contiguous.
 *   Output is not padded to a multiple of 252 bytes in size.
 * @author David Knoll <david@davidknoll.me.uk>
 * @copyright MIT License
 * @date 23/07/2015
 */
#include <stdio.h>
#include <stdlib.h>

/**
 * @fn unsigned char finhn(FILE *f)
 * @brief Inputs an ASCII hex nibble.
 * @param f Open file pointer to the input S-record file.
 * @return The binary representation of the hex nibble input.
 */
unsigned char finhn(FILE *f)
{
  unsigned char c;
  // Skips over non-hex characters, this isn't ideal
  do {
    c = fgetc(f);
  } while (
    !(c >= '0' && c <= '9') &&
    !(c >= 'A' && c <= 'F')
  );
  if (c <= '9') {
    return c - '0';
  } else {
    return c + 0x0A - 'A';
  }
}

/**
 * @fn unsigned char finhb(FILE *f)
 * @brief Inputs an ASCII hex byte.
 * @param f Open file pointer to the input S-record file.
 * @return The binary representation of the hex byte input.
 */
unsigned char finhb(FILE *f)
{
  unsigned char result;
  result = finhn(f) << 4;
  result |= finhn(f);
  return result;
}

/**
 * @fn unsigned int finhw(FILE *f)
 * @brief Inputs an ASCII hex word.
 * @param f Open file pointer to the input S-record file.
 * @return The binary representation of the hex word input.
 */
unsigned int finhw(FILE *f)
{
  unsigned int result;
  result = finhb(f) << 8;
  result |= finhb(f);
  return result;
}

/**
 * @fn int record(FILE *infile, FILE *outfile)
 * @brief
 *   Processes one record from the input file to the output file.
 * @details
 *   NUL/CR/LF between records are skipped over.
 *   Anything else between records is an error.
 *   S0/5 records are skipped over.
 *   Unrecognised, S2-3 or S7-8 records are considered an error.
 * @param infile Open file pointer to the input S-record file.
 * @param outfile Open file pointer to the output FLEX binary.
 * @return
 *   The S-record type processed.
 *   Most likely '0', '1', '5' or '9'.
 *   EOF if encountered.
 *   'S' if bad data between records.
 *   'R' if unrecognised or unacceptable record type.
 *   'C' if recognised record type but bad checksum.
 */
int record(FILE *infile, FILE *outfile)
{
  int rectype, nbytes, c;
  unsigned int chksum, loadaddr;

  do { // S
    c = fgetc(infile);
    // If EOF, return now
    if (c == EOF) return EOF;
    // Skip over null, CR, LF. Anything other than those or S is an error.
    if (c != 0x00 && c != 0x0D && c != 0x0A && c != 'S') return 'S';
  } while (c != 'S');

  rectype = fgetc(infile); // 0-9
  c = finhb(infile); // Count
  chksum = c;
  nbytes = c - 3;
  loadaddr = finhw(infile); // Address
  chksum += (loadaddr >> 8) & 0xFF;
  chksum += loadaddr & 0xFF;
  switch (rectype) {

    case '1': // Data
      if (!nbytes) break; // Skip empty records
      fputc(0x02, outfile);
      fputc((loadaddr >> 8) & 0xFF, outfile);
      fputc(loadaddr & 0xFF, outfile);
      fputc(nbytes, outfile);
      // Loop over data bytes in the record
      while (nbytes--) {
        c = finhb(infile);
        chksum += c;
        fputc(c, outfile);
      }
      break;

    case '9': // Start address
      if (!loadaddr) break; // Skip null addresses
      fputc(0x16, outfile);
      fputc((loadaddr >> 8) & 0xFF, outfile);
      fputc(loadaddr & 0xFF, outfile);
      break;

    case '0': // Header
    case '5': // Count
      // Skip over, but update checksum anyway
      while (nbytes--) {
        c = finhb(infile);
        chksum += c;
      }
      break;

    default: // Unrecognised or unacceptable record type
      return 'R';
  }
  chksum += finhb(infile); // Checksum
  if (chksum & 0xFF != 0xFF) return 'C';
  return rectype;
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
  int rectype;

  if (argc != 3) {
    fprintf(stderr, "\
Motorola S-record to FLEX binary converter\n\
Usage: %s infile outfile\n\
Output records are the same size as input records, so may not\n\
be as large as possible even where data is contiguous.\n\
Output is not padded to a multiple of 252 bytes in size.\n\
", argv[0]);
    return EXIT_FAILURE;
  }

  // Open files for input and output
  infile = fopen(argv[1], "rt");
  outfile = fopen(argv[2], "wb");
  if (infile == NULL) {
    fprintf(stderr, "Error opening file %s for input.\n", argv[1]);
  } else if (outfile == NULL) {
    fprintf(stderr, "Error opening file %s for output.\n", argv[2]);

  } else {
    // Loop processing records until EOF or error
    do {
      rectype = record(infile, outfile);
    } while (rectype == '0' || rectype == '1' || rectype == '5' || rectype == '9');

    if (rectype != EOF) {
      fprintf(stderr, "Error %c before offset %04X in input file.\n", rectype, (int) ftell(infile));
    }
  }

  if (infile != NULL) fclose(infile);
  if (outfile != NULL) fclose(outfile);
  return (rectype == EOF) ? EXIT_SUCCESS : EXIT_FAILURE;
}
