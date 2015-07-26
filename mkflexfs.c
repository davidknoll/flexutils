/**
 * @file mkflexfs.c
 * @brief FLEX blank disk image creator
 * @details See ./mkflexfs -h for usage
 * @author David Knoll <david@davidknoll.me.uk>
 * @copyright MIT License
 * @date 26/07/2015
 */
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

FILE *outfile;
int tracks = 77, sectors = 15, volnum = 0;
char *volname = "";

/**
 * @fn void outblank(int ltrk, int lsect)
 * @brief Outputs a blank sector with a link to the next in the chain.
 * @details Pass zero in both parameters to output a sector of all zeroes.
 * @param ltrk Track number of the next sector in the chain.
 * @param lsect Sector number of the next sector in the chain.
 */
void outblank(int ltrk, int lsect)
{
  int i;
  fputc(ltrk, outfile);
  fputc(lsect, outfile);
  for (i = 0; i < 254; i++) fputc(0, outfile);
}

/**
 * @fn void outsir(void)
 * @brief Outputs a System Information Record sector (track 0, sector 3).
 * @details
 *   Date is taken from the system date. Track and sector count,
 *   volume name and volume number are taken from the global variables.
 */
void outsir(void)
{
  int i;
  time_t rawtime;
  struct tm *timeinfo;
  // Zeroes
  for (i = 0; i < 16; i++) fputc(0, outfile);

  // Disk name, volume number
  fputs(volname, outfile);
  for (i = 0; i < 11 - strlen(volname); i++) fputc(0, outfile);
  fputc(volnum >> 8, outfile);
  fputc(volnum, outfile);

  // Start, end, size of free chain
  fputc(1, outfile);
  fputc(1, outfile);
  fputc(tracks - 1, outfile);
  fputc(sectors, outfile);
  fputc(((tracks - 1) * sectors) >> 8, outfile);
  fputc(((tracks - 1) * sectors), outfile);

  // Initialisation date (mm/dd/yy)
  time(&rawtime);
  timeinfo = localtime(&rawtime);
  fputc(timeinfo->tm_mon + 1, outfile);
  fputc(timeinfo->tm_mday, outfile);
  fputc(timeinfo->tm_year % 100, outfile);

  // Max track/sector number
  fputc(tracks - 1, outfile);
  fputc(sectors, outfile);

  // Reserved
  for (i = 0; i < 216; i++) fputc(0, outfile);
}

/**
 * @fn void outsector(int track, int sector)
 * @brief Outputs sector contents of the correct type for the specified sector.
 * @param track
 * @param sector
 */
void outsector(int track, int sector)
{
  if (track == 0 && sector == 3) {
    // System Information Record
    outsir();

  } else if (track == 0 && sector >= 5 && sector < sectors) {
    // Directory chain
    outblank(track, sector + 1);
  } else if (track == 0) {
    // Boot sector (1-2), reserved (4), or end of directory chain
    outblank(0, 0);

  } else if (track == tracks - 1 && sector == sectors) {
    // End of free chain
    outblank(0, 0);
  } else if (sector == sectors) {
    // End of track
    outblank(track + 1, 1);
  } else {
    // Free chain
    outblank(track, sector + 1);
  }
}

/**
 * @fn void usage(const char *cmd)
 * @brief Outputs help for the command.
 * @param cmd Name of the program file, most likely from argv[0].
 */
void usage(const char *cmd)
{
  fprintf(stderr, "\
FLEX blank disk image creator\n\
Usage: %s [-t tracks] [-s sectors] [-n volname] [-v volnum] [-o filename] [-h]\n\
\ttracks is an integer, default 77, min 2\n\
\tsectors is an integer, default 15, min 5\n\
\tvolname is max 11 characters, default empty\n\
\tvolnum is an integer, default 0\n\
\tfilename may be (and defaults to) -, but won't output to the terminal\n\
\t-h prints this message\n\
", cmd);
  exit(EXIT_FAILURE);
}

/**
 * @fn int main(int argc, char *argv[])
 * @brief Main function
 * @details Open/close files and iterate through sectors to output
 * @param argc Command-line argument count
 * @param argv Command-line arguments
 * @return Zero on success, non-zero on error
 */
int main(int argc, char *argv[])
{
  int opt, trk, sec;
  char *outfilename = "-";

  while ((opt = getopt(argc, argv, "t:s:n:v:o:h")) != -1) {
    switch (opt) {
      case 't': // Tracks
        tracks = atoi(optarg);
        if (tracks < 2) usage(argv[0]);
        break;
      case 's': // Sectors
        sectors = atoi(optarg);
        if (sectors < 5) usage(argv[0]);
        break;

      case 'n': // Volume name
        volname = optarg;
        if (strlen(volname) > 11) usage(argv[0]);
        break;
      case 'v': // Volume number
        volnum = atoi(optarg);
        break;

      case 'o': // Output filename
        outfilename = optarg;
        break;

      case 'h': // Help/usage
      case '?':
      default:
        usage(argv[0]);
    }
  }

  if (strcmp("-", outfilename)) {
    // Output to file
    outfile = fopen(outfilename, "wb");
    if (outfile == NULL) {
      fprintf(stderr, "Error opening file %s for output, errno %d\n", outfilename, errno);
      return EXIT_FAILURE;
    }
  } else {
    // Output to stdout, if it isn't the terminal
    if (isatty(1)) usage(argv[0]);
    outfile = stdout;
  }

  // Output each sector in turn
  for (trk = 0; trk < tracks; trk++) {
    for (sec = 1; sec <= sectors; sec++) {
      outsector(trk, sec);
    }
  }
  if (outfile != stdout) fclose(outfile);
  return EXIT_SUCCESS;
}
