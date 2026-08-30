#ifndef UZ_WORKFLOW_H
#define UZ_WORKFLOW_H

#define UZ_WORKFLOW_OK        0u
#define UZ_WORKFLOW_STATE     1u
#define UZ_WORKFLOW_DOS       2u
#define UZ_WORKFLOW_OUTPUT    3u
#define UZ_WORKFLOW_CATALOG   4u
#define UZ_WORKFLOW_INPUT     5u
#define UZ_WORKFLOW_CREATE    6u
#define UZ_WORKFLOW_PREFLIGHT 7u
#define UZ_WORKFLOW_EXTRACT   8u
#define UZ_WORKFLOW_CLOSE     9u
#define UZ_WORKFLOW_COMMIT   10u
#define UZ_WORKFLOW_VERIFY   11u
#define UZ_WORKFLOW_CANCEL   12u

/* Detail codes used while UZ_WORKFLOW_DOS is active.  Keep these as compact
 * transaction checkpoints so a physical-machine failure identifies the one
 * Ultimate DOS boundary that rejected the request. */
#define UZ_WORKFLOW_DOS_IDENTIFY_INPUT  1u
#define UZ_WORKFLOW_DOS_IDENTIFY_OUTPUT 2u
#define UZ_WORKFLOW_DOS_SPLIT_INPUT     3u
#define UZ_WORKFLOW_DOS_CD_INPUT        4u
#define UZ_WORKFLOW_DOS_OPEN_INPUT      5u
#define UZ_WORKFLOW_DOS_INFO_INPUT      6u
#define UZ_WORKFLOW_DOS_CD_OUTPUT       7u

typedef unsigned char (*UzWorkflowProgress)(void *context,
                                             unsigned int completed,
                                             unsigned int total,
                                             const char *name);

/* The catalog already contains a fully expanded, breadth-first Create plan.
 * Each member is one safe cancellation boundary. The output is written to a
 * unique sibling, reopened through the frozen ZIP reader, and renamed only
 * after the complete archive verifies. */
unsigned char uz_workflow_create(unsigned char package_bank,
                                 unsigned char work_bank,
                                 unsigned char catalog_bank,
                                 const char *source_base,
                                 const char *output_dir,
                                 const char *output_name,
                                 unsigned int entry_count,
                                 UzWorkflowProgress progress,
                                 void *progress_context);

unsigned char uz_workflow_extract(unsigned char package_bank,
                                  unsigned char work_bank,
                                  unsigned char catalog_bank,
                                  const char *archive_path,
                                  const char *destination_root,
                                  UzWorkflowProgress progress,
                                  void *progress_context);

unsigned char uz_workflow_error(void);
unsigned char uz_workflow_detail(void);
unsigned int uz_workflow_completed(void);

#endif
