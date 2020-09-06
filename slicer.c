#include "stdafx.h"
#include "LoftyCAD.h"
#include <stdio.h>
#include <ShlObj.h>

// Slic3r interface.

// List of locations to look for Slic3r. If PrusaSlicer is installed, assume that takes priority
// over vanilla slic3r. The leading backslash allows cat'ing with the program files directory.
// Have (x86) versions here too.
// Directory names may have version stuff appended, so search using a wildcard.
char* slic3r_locations[NUM_SLICER_LOCATIONS] =
{
    "\\Prusa3D\\Slic3rPE",
    "\\Slic3r*",
    " (x86)\\Prusa3D\\Slic3rPE",
    " (x86)\\Slic3r*"
};

// List of possible locations for config files under Application Data. No wildcards.
char* config_locations[NUM_CONFIG_LOCATIONS] =
{
    "\\Slic3rPE",
    "\\Slic3r"
};

// Arrays of exe and config names, and the index to the current one of each.
char slicer_exe[MAX_SLICERS][MAX_PATH];
char slicer_config[MAX_SLICERS][MAX_PATH];
int slicer_index = 0;
int config_index = 0;
int num_slicers = 0;
int num_configs = 0;

// Load Slic3r executable and config directories from the reg. Return FALSE if we don't have an exe
// and leave the fields blank.
BOOL
load_slic3r_exe_and_config()
{
    HKEY hkey;
    char str[MAX_PATH];
    char location[32];
    int len, i;
    BOOL rc = FALSE;

    RegCreateKeyEx(HKEY_CURRENT_USER, "Software\\LoftyCAD\\Slic3r", 0, NULL, 0, KEY_READ, NULL, &hkey, NULL);

    num_slicers = 0;
    num_configs = 0;
    slicer_index = 0;
    config_index = 0;

    slicer_exe[0][0] = '\0';
    slicer_config[0][0] = '\0';

    for (i = 0; i < MAX_SLICERS; i++)
    {
        sprintf_s(location, 32, "Location%d", i);
        len = MAX_PATH;
        if (RegQueryValueEx(hkey, location, 0, NULL, str, &len) != ERROR_SUCCESS)
            break;
        rc = TRUE;
        strcpy_s(slicer_exe[i], MAX_PATH, str);
        num_slicers++;
    }

    // Haven't got anything stored - try and find some.
    if (!rc)
    {
        RegCloseKey(hkey);
        return find_slic3r_exe_and_config();
    }

    // Look for configs and the indices
    for (i = 0; i < MAX_SLICERS; i++)
    {
        sprintf_s(location, 32, "ConfigDir%d", i);
        len = MAX_PATH;
        if (RegQueryValueEx(hkey, location, 0, NULL, str, &len) != ERROR_SUCCESS)
            break;
        strcpy_s(slicer_config[i], MAX_PATH, str);
        num_configs++;
    }

    // Read the indices (they are DWORDs rather than strings)
    len = 4;
    RegQueryValueEx(hkey, "SlicerIndex", 0, NULL, (LPBYTE)&slicer_index, &len);
    len = 4;
    RegQueryValueEx(hkey, "ConfigIndex", 0, NULL, (LPBYTE)&config_index, &len);
    RegCloseKey(hkey);

    return TRUE;
}

// Save slicer stuff to registry.
void
save_slic3r_exe_and_config()
{
    int i;
    char location[32];
    HKEY hkey;

    RegCreateKeyEx(HKEY_CURRENT_USER, "Software\\LoftyCAD\\Slic3r", 0, NULL, 0, KEY_ALL_ACCESS, NULL, &hkey, NULL);

    for (i = 0; i < num_slicers; i++)
    {
        sprintf_s(location, 32, "Location%d", i);
        RegSetValueEx(hkey, location, 0, REG_SZ, slicer_exe[i], strlen(slicer_exe[i]) + 1);
    }
    for (; i < MAX_SLICERS; i++)        // clear out any old ones beyond the num
    {
        sprintf_s(location, 32, "Location%d", i);
        RegDeleteKeyValue(hkey, NULL, location);
    }

    for (i = 0; i < num_configs; i++)
    {
        sprintf_s(location, 32, "ConfigDir%d", i);
        RegSetValueEx(hkey, location, 0, REG_SZ, slicer_config[i], strlen(slicer_config[i]) + 1);
    }
    for (; i < MAX_SLICERS; i++)        // clear out any old ones beyond the num
    {
        sprintf_s(location, 32, "ConfigDir%d", i);
        RegDeleteKeyValue(hkey, NULL, location);
    }

    RegSetValueEx(hkey, "SlicerIndex", 0, REG_DWORD, (LPBYTE)&slicer_index, 4);
    RegSetValueEx(hkey, "ConfigIndex", 0, REG_DWORD, (LPBYTE)&config_index, 4);
    RegCloseKey(hkey);
}


// Find Slic3r executable and config directories in standard places (for Slic3r and PrusaSlicer).
// Return FALSE if an exe was not found. 

// exe - could be anywhere, but look in list of locations under Program Files.
// config - look in <user>\AppData\Roaming\Slic3r[PE]
BOOL
find_slic3r_exe_and_config()
{
    char progfiles[MAX_PATH];
    char appdata[MAX_PATH];
    char location[MAX_PATH];
    char filename[MAX_PATH];
    int i, len;
    char* slosh;
    HANDLE h, f;
    WIN32_FIND_DATA find_data;
    BOOL rc = FALSE;

    num_slicers = 0;
    num_configs = 0;
    slicer_index = 0;
    config_index = 0;

    // Get the Program Files folder name.
    SHGetFolderPath(NULL, CSIDL_PROGRAM_FILES, NULL, SHGFP_TYPE_CURRENT, progfiles);

    // Slic3r locations may be in Program Files, or Program Files (x86).
    // For some reason (I am a 32-bit program maybe?) the above call may return
    // the (x86) folder even if it's running on a 64-bit system. Strip this if present.
    // In addition, the Slic3r directory name may have version stuff appended.
    len = strlen(progfiles);
    if (_strcmpi(&progfiles[len - 6], " (x86)") == 0)
        progfiles[len - 6] = '\0';

    for (i = 0; i < NUM_SLICER_LOCATIONS; i++)
    {
        strcpy_s(location, MAX_PATH, progfiles);
        strcat_s(location, MAX_PATH, slic3r_locations[i]);
        len = strlen(location);

        h = FindFirstFile(location, &find_data);
        if (h == INVALID_HANDLE_VALUE)
            continue;
        if (!(find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
            continue;

        while (1)
        {
            strcpy_s(filename, MAX_PATH, location);

            // Replace the directory wildcard with its real name and look for slic3r-console.exe
            slosh = strrchr(filename, '\\');
            *(slosh + 1) = '\0';
            strcat_s(filename, MAX_PATH, find_data.cFileName);
            strcat_s(filename, MAX_PATH, "\\slic3r-console.exe");
            f = CreateFile(filename, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
            if (f != INVALID_HANDLE_VALUE)
            {
                // Found one - store it away filename and all.
                strcpy_s(slicer_exe[num_slicers++], MAX_PATH, filename);
                rc = TRUE;
                CloseHandle(f);
            }
            if (FindNextFile(h, &find_data) == 0)
                break;
        }

        FindClose(h);
    }

    // Get the Application Data folder name for the current user.
    SHGetFolderPath(NULL, CSIDL_APPDATA, NULL, SHGFP_TYPE_CURRENT, appdata);

    for (i = 0; i < NUM_CONFIG_LOCATIONS; i++)
    {
        strcpy_s(location, MAX_PATH, appdata);
        strcat_s(location, MAX_PATH, config_locations[i]);

        // slic3r.ini must be present.
        strcpy_s(filename, MAX_PATH, location);
        strcat_s(filename, MAX_PATH, "\\slic3r.ini");
        f = CreateFile(filename, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        if (f != INVALID_HANDLE_VALUE)
        {
            // Found it. Just the directory is stored.
            strcpy_s(slicer_config[num_configs++], MAX_PATH, location);
            CloseHandle(f);
        }
    }

    // Put the stuff we have found into the registry. Assume the selected index is 0 for now.
    save_slic3r_exe_and_config();

    return rc;
}

// Read Slic3r configuration and populate allowable printers, print settings and filament settings.






// Slice the object tree and produce a G-code file.

