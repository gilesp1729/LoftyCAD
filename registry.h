// Registry functions

#ifndef __REG_H__
#define __REG_H__

void load_MRU_to_menu(HMENU hMenu);
void insert_filename_to_MRU(HMENU hMenu, char *filename);
BOOL get_filename_from_MRU(int id, char *filename);
void remove_filename_from_MRU(HMENU hMenu, char *filename);

#define ID_MRU_BASE 60000
#define ID_MRU_FILE1 60001
#define ID_MRU_FILE2 60002
#define ID_MRU_FILE3 60003
#define ID_MRU_FILE4 60004

#endif // __REG_H__