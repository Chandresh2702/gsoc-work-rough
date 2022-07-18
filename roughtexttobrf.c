#include "liblouis.h"

/*
 * 'cfFiltertextobrf()' - Filter function to convert txt input into
 *               brf to be printed on braille printers
 */

int                          /* O - Error status */
cfFiltertextTobrf(int inputfd,         /* I - File descriptor input stream */
	int outputfd,        /* I - File descriptor output stream */
	int inputseekable,   /* I - Is input stream seekable? (unused) */
	cf_filter_data_t *data, /* I - Job and printer data */
	void *parameters)    /* I - Filter-specific parameters (unused) */
{
  
  FILE          *inputfp;		/* Input file pointer */
  int		fd = 0;			/* Copy file descriptor */
  int           i, j;
  int		pdftopdfapplied = 0;    /* Input data from pdftopdf filter? */
  char		deviceCopies[32] = "1"; /* Hardware copies */
  int		deviceCollate = 0;      /* Hardware collate */
  char          make_model[128] = "";   /* Printer make and model (for quirks)*/
  char		*filename,		/* PDF file to convert */
		tempfile[1024];		/* Temporary file */
  char		buffer[8192];		/* Copy buffer */
  int		bytes;			/* Bytes copied */
  int		num_options = 0,		/* Number of options */
                num_pstops_options;	/* Number of options for pstops */
  cups_option_t	*options = NULL,		/* Options */
                *pstops_options,	/* Options for pstops filter function */
                *option;
  const char    *exclude;
  cf_filter_data_t pstops_filter_data;
  int           ret;
  const char	*val;			/* Option value */
  ppd_file_t	*ppd;			/* PPD file */
  char		resolution[128] = "";   /* Output resolution */
  int           xres = 0, yres = 0,     /* resolution values */
                mres, res,
                maxres = CUPS_PDFTOPS_MAX_RESOLUTION,
                                        /* Maximum image rendering resolution */
                numvalues;              /* Number of values actually read */
  ppd_choice_t  *choice;
  ppd_attr_t    *attr;
  cups_page_header2_t header;
  cups_file_t	*fp;			/* Post-processing input file */
  int		pdf_pid,		/* Process ID for pdftops/gs */
		pdf_argc = 0,		/* Number of args for pdftops/gs */
		pstops_pid = 0,		/* Process ID of pstops filter */
		pstops_pipe[2],		/* Pipe to pstops filter */
		need_post_proc = 0,     /* Post-processing needed? */
		post_proc_pid = 0,	/* Process ID of post-processing */
		post_proc_pipe[2],	/* Pipe to post-processing */
		wait_children,		/* Number of child processes left */
		wait_pid,		/* Process ID from wait() */
		wait_status,		/* Status from child */
		exit_status = 0;	/* Exit status */
  int gray_output = 0; /* Checking for monochrome/grayscale PostScript output */
  char		*pdf_argv[100],		/* Arguments for pdftops/gs */
		*ptr;			/* Pointer into value */
  int		duplex, tumble;         /* Duplex settings for PPD-less
					   printing */
  cf_logfunc_t log = data->logfunc;
  void          *ld = data->logdata;
  cf_filter_iscanceledfunc_t iscanceled = data->iscanceledfunc;
  void          *icd = data->iscanceleddata;
  ipp_t *printer_attrs = data->printer_attrs;
  ipp_t *job_attrs = data->job_attrs;
  ipp_attribute_t *ipp;


  (void)inputseekable;
  (void)parameters;

 /*
  * Ignore broken pipe signals...
  */

  signal(SIGPIPE, SIG_IGN);

 /*
  * Open the input data stream specified by the inputfd...
  */

  if ((inputfp = fdopen(inputfd, "r")) == NULL)
  {
    if (!iscanceled || !iscanceled(icd))
    {
      if (log) log(ld, CF_LOGLEVEL_DEBUG,
		   "cfFilterPDFToPS: Unable to open input data stream.");
    }

    return (1);
  }

 /*
  * Copy input into temporary file ...
  */

  if ((fd = cupsTempFd(tempfile, sizeof(tempfile))) < 0)
  {
    if (log) log(ld, CF_LOGLEVEL_ERROR,
		 "cfFilterPDFToPS: Unable to copy PDF file: %s", strerror(errno));
    return (1);
  }

  if (log) log(ld, CF_LOGLEVEL_DEBUG,
	       "cfFilterPDFToPS: Copying input to temp file \"%s\"",
	       tempfile);

  while ((bytes = fread(buffer, 1, sizeof(buffer), inputfp)) > 0)
    bytes = write(fd, buffer, bytes);

  if (inputfd)
  {
    fclose(inputfp);
    close(inputfd);
  }
  close(fd);

  filename = tempfile;
