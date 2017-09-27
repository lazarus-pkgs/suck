#ifndef _SUCK_DB_H
#define _SUCK_DB_H

/* function prototypes */
int db_delete(PMaster);
int db_write(PMaster);
int db_read(PMaster);
int db_mark_dled(PMaster, PList);
int db_open(PMaster);
void db_close(PMaster);

#endif /* _SUCK_DB_H */
