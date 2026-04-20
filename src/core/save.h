#pragma once

#define MAX_SAVES 16

/* Creates save_NNN.dat with the next available number. Returns 1 on success. */
int saveGameNew(void);

/* Saves with a user-provided display name (sanitized into the filename). Returns 1 on success. */
int saveGameAs(const char *name);

/* Loads a specific save file by name (e.g. "save_001.dat"). Returns 1 on success. */
int loadGameNamed(const char *filename);

/* Fills names[][64] with save filenames found on disk. Returns count. */
int listSaves(char names[][64], int maxCount);

/* Returns 1 if at least one save file exists. */
int anySaveExists(void);
