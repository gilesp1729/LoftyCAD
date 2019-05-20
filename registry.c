#include "stdafx.h"
#include "LoftyCAD.h"

#define VALEN 256

// registry functions for dealing with MRU lists

// Read the MRU list from the registry and append it to the File menu
void
load_MRU_to_menu(HMENU hMenu)
{
    HKEY hkey;
    char str[VALEN];
    char file1[VALEN];
    char file2[VALEN];
    char file3[VALEN];
    char file4[VALEN];
    int len;

    RegCreateKeyEx(HKEY_CURRENT_USER, "Software\\LoftyCAD\\MRUList", 0, NULL, 0, KEY_READ, NULL, &hkey, NULL);

    len = VALEN;
    if (RegQueryValueEx(hkey, "File1", 0, NULL, str, &len) == ERROR_SUCCESS)
    {
        strcpy_s(file1, VALEN, "&1 ");
        strncat_s(file1, VALEN, str,  len);
        AppendMenu(hMenu, 0, ID_MRU_FILE1, file1);
    }
    len = VALEN;
    if (RegQueryValueEx(hkey, "File2", 0, NULL, str, &len) == ERROR_SUCCESS)
    {
        strcpy_s(file2, VALEN, "&2 ");
        strncat_s(file2, VALEN, str, len);
        AppendMenu(hMenu, 0, ID_MRU_FILE2, file2);
    }
    len = VALEN;
    if (RegQueryValueEx(hkey, "File3", 0, NULL, str, &len) == ERROR_SUCCESS)
    {
        strcpy_s(file3, VALEN, "&3 ");
        strncat_s(file3, VALEN, str, len);
        AppendMenu(hMenu, 0, ID_MRU_FILE3, file3);
    }
    len = VALEN;
    if (RegQueryValueEx(hkey, "File4", 0, NULL, str, &len) == ERROR_SUCCESS)
    {
        strcpy_s(file4, VALEN, "&4 ");
        strncat_s(file4, VALEN, str, len);
        AppendMenu(hMenu, 0, ID_MRU_FILE4, file4);
    }

    RegCloseKey(hkey);
}

// Shuffle down the MRU list. Copy 3-4, 2-3 and 1-2. The values are known to exist.
void
shuffle_down(HMENU hMenu, HKEY hkey)
{
    char str[VALEN];
    char file2[VALEN];
    char file3[VALEN];
    char file4[VALEN];
    int len;

    len = VALEN;
    RegQueryValueEx(hkey, "File3", 0, NULL, str, &len);
    RegSetKeyValue(hkey, NULL, "File4", REG_SZ, str, len);
    strcpy_s(file4, VALEN, "&4 ");
    strcat_s(file4, VALEN, str);
    ModifyMenu(hMenu, 4, MF_BYPOSITION, ID_MRU_FILE4, file4);

    len = VALEN;
    RegQueryValueEx(hkey, "File2", 0, NULL, str, &len);
    RegSetKeyValue(hkey, NULL, "File3", REG_SZ, str, len);
    strcpy_s(file3, VALEN, "&3 ");
    strcat_s(file3, VALEN, str);
    ModifyMenu(hMenu, 3, MF_BYPOSITION, ID_MRU_FILE3, file3);

    len = VALEN;
    RegQueryValueEx(hkey, "File1", 0, NULL, str, &len);
    RegSetKeyValue(hkey, NULL, "File2", REG_SZ, str, len);
    strcpy_s(file2, VALEN, "&2 ");
    strcat_s(file2, VALEN, str);
    ModifyMenu(hMenu, 2, MF_BYPOSITION, ID_MRU_FILE2, file2);
}


// Insert a new filename to the MRU list
void
insert_filename_to_MRU(HMENU hMenu, char *filename)
{
    HKEY hkey;
    char str[VALEN];
    char file1[VALEN];
    char file2[VALEN];
    char file3[VALEN];
    char file4[VALEN];
    int len;

    RegCreateKeyEx(HKEY_CURRENT_USER, "Software\\LoftyCAD\\MRUList", 0, NULL, 0, KEY_ALL_ACCESS, NULL, &hkey, NULL);

    // Try for a free slot. If we find the filename already in the list, stop there.
    len = VALEN;
    if (RegQueryValueEx(hkey, "File1", 0, NULL, str, &len) == ERROR_SUCCESS)
    {
        if (strcmp(str, filename) == 0)
            return;
    }
    else // Nothing in this slot, insert it here and return
    {
        RegSetKeyValue(hkey, NULL, "File1", REG_SZ, filename, strlen(filename) + 1);
        strcpy_s(file1, VALEN, "&1 ");
        strcat_s(file1, VALEN, filename);
        AppendMenu(hMenu, 0, ID_MRU_FILE1, file1);
        return;
    }

    // Continue with the remaining slots.
    len = VALEN;
    if (RegQueryValueEx(hkey, "File2", 0, NULL, str, &len) == ERROR_SUCCESS)
    {
        if (strcmp(str, filename) == 0)
            return;
    }
    else // Nothing in this slot, insert it here and return
    {
        RegSetKeyValue(hkey, NULL, "File2", REG_SZ, filename, strlen(filename) + 1);
        strcpy_s(file2, VALEN, "&2 ");
        strcat_s(file2, VALEN, filename);
        AppendMenu(hMenu, 0, ID_MRU_FILE2, file2);
        return;
    }

    len = VALEN;
    if (RegQueryValueEx(hkey, "File3", 0, NULL, str, &len) == ERROR_SUCCESS)
    {
        if (strcmp(str, filename) == 0)
            return;
    }
    else // Nothing in this slot, insert it here and return
    {
        RegSetKeyValue(hkey, NULL, "File3", REG_SZ, filename, strlen(filename) + 1);
        strcpy_s(file3, VALEN, "&3 ");
        strcat_s(file3, VALEN, filename);
        AppendMenu(hMenu, 0, ID_MRU_FILE3, file3);
        return;
    }

    len = VALEN;
    if (RegQueryValueEx(hkey, "File4", 0, NULL, str, &len) == ERROR_SUCCESS)
    {
        if (strcmp(str, filename) == 0)
            return;

        // Shuffle the list down and insert the new one at the top.
        shuffle_down(hMenu, hkey);
        RegSetKeyValue(hkey, NULL, "File1", REG_SZ, filename, strlen(filename) + 1);
        strcpy_s(file1, VALEN, "&1 ");
        strcat_s(file1, VALEN, filename);
        ModifyMenu(hMenu, 1, MF_BYPOSITION, ID_MRU_FILE1, file1);
    }
    else // Nothing in this slot, insert it here and return
    {
        RegSetKeyValue(hkey, NULL, "File4", REG_SZ, filename, strlen(filename) + 1);
        strcpy_s(file4, VALEN, "&4 ");
        strcat_s(file4, VALEN, filename);
        AppendMenu(hMenu, 0, ID_MRU_FILE4, file4);
    }

    RegCloseKey(hkey);
}

// Get a filename from the MRU list
BOOL
get_filename_from_MRU(int id, char *filename)
{
    HKEY hkey;
    char str[VALEN];
    int len;
    char *keyval[] = { "File1", "File2", "File3", "File4" };

    RegCreateKeyEx(HKEY_CURRENT_USER, "Software\\LoftyCAD\\MRUList", 0, NULL, 0, KEY_READ, NULL, &hkey, NULL);

    len = VALEN;
    if (RegQueryValueEx(hkey, keyval[id-1], 0, NULL, str, &len) == ERROR_SUCCESS)
    {
        strcpy_s(filename, VALEN, str);
        RegCloseKey(hkey);
        return TRUE;
    }

    RegCloseKey(hkey);
    return FALSE;
}

// Remove a file from the MRU list
void
remove_filename_from_MRU(HMENU hMenu, int id)
{
    HKEY hkey;
    char *keyval[] = { "File1", "File2", "File3", "File4" };

    RegCreateKeyEx(HKEY_CURRENT_USER, "Software\\LoftyCAD\\MRUList", 0, NULL, 0, KEY_ALL_ACCESS, NULL, &hkey, NULL);
    RegDeleteKeyValue(hkey, NULL, keyval[id-1]);

    // Just clear and reload the lot. It's easier than a conditional shuffle-up.
    DeleteMenu(hMenu, 4, MF_BYPOSITION);
    DeleteMenu(hMenu, 3, MF_BYPOSITION);
    DeleteMenu(hMenu, 2, MF_BYPOSITION);
    DeleteMenu(hMenu, 1, MF_BYPOSITION);
    RegCloseKey(hkey);
    load_MRU_to_menu(hMenu);
}
