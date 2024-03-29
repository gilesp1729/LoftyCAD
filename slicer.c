#include "stdafx.h"
#include "LoftyCAD.h"
#include <stdio.h>
#include <ShlObj.h>

// Slic3r interface.

// Size of a section name.
#define SECT_NAME_SIZE      80

// Max number of key=value pairs in a section. Note that inheritances may make
// this requirement much larger than the final output as dups are culled later.
#define MAX_KEYVALS         512

// Max size of a section in characters, with null-separated key=value pairs.
#define MAX_SECT_SIZE       8192

// Max size of the section name buffer
#define MAX_SECT_NAME_SIZE  32767

// Max number of section in the vendor ini file
#define MAX_SECTIONS        1024

// Inheritable string and section structures.
typedef struct InhString
{
    char*       key;                    // Pointer to key=value in section string
    char*       value;                  // Pointer to value in section string
    BOOL        override;               // TRUE is this key has been overridden
} InhString;

// Section structure. 
typedef struct InhSection
{
    char        sect_name[SECT_NAME_SIZE];      // The original name of the section from the ini file.
    int         n_keyvals;                      // Number of key/value pairs.
    float       bed_xmin;                       // The bed size for a printer section
    float       bed_xmax;
    float       bed_ymin;
    float       bed_ymax;
    InhString   keyval[MAX_KEYVALS];            // Key/value pairs inside the section_string.
    char        section_string[MAX_SECT_SIZE];  // Null-separated raw string, returned from GetPrivateProfileSection.
} InhSection;

// Slicer and config fixed arrays
#define NUM_SLICER_LOCATIONS    10
#define NUM_CONFIG_LOCATIONS    4

// Location structure used for executables and ini files. Allows marking any location 
// as PrusaSlicer, Slic3rPE, or Slic3r to discourage accidentally pointing a slicer at
// an incompatible set of config files.
typedef struct Location
{
    char        directory[64];                  // Subdirectory under ProgramFiles or AppData\Roaming
    char        target[64];                     // The target exe or ini file
    SLICER      type;                           // What kind of slicer it is
} Location;

// List of locations to look for Slic3r. If PrusaSlicer is installed, assume that takes priority
// over vanilla slic3r. The leading backslash allows cat'ing with the program files directory.
// Have (x86) versions here too.
// Directory names may have version stuff appended, so search using a wildcard.
Location slic3r_locations[NUM_SLICER_LOCATIONS] =
{
    {"\\Prusa3D\\PrusaSlicer",      "prusa-slicer-console.exe", SLIC_PRUSA },
    {"\\Prusa3D\\Slic3rPE",         "slic3r-console.exe",       SLIC_SLIC3R},
    {"\\Repetier-Host\\Slic3rPE",   "prusa-slicer-console.exe", SLIC_PRUSA},
    {"\\Repetier-Host\\Slic3r",     "slic3r-console.exe",       SLIC_SLIC3R},
    {"\\Slic3r*",                   "slic3r-console.exe",       SLIC_SLIC3R},
    {" (x86)\\Prusa3D\\PrusaSlicer","prusa-slicer-console.exe", SLIC_PRUSA },
    {" (x86)\\Prusa3D\\Slic3rPE",   "slic3r-console.exe",       SLIC_SLIC3R},
    {" (x86)\\Repetier-Host\\Slic3rPE", "prusa-slicer-console.exe", SLIC_PRUSA},
    {" (x86)\\Repetier-Host\\Slic3r", "slic3r-console.exe",     SLIC_SLIC3R},
    {" (x86)\\Slic3r*",             "slic3r-console.exe",       SLIC_SLIC3R}
};

// List of possible locations for config files under Application Data. No wildcards.
// Some Prusa installs (e.g. Repetier) use PrusaSlicer.ini under the old directory name.
Location config_locations[NUM_CONFIG_LOCATIONS] =
{
    {"\\PrusaSlicer",                "PrusaSlicer.ini",         SLIC_PRUSA },
    {"\\Slic3rPE",                   "PrusaSlicer.ini",         SLIC_PRUSA },
    {"\\Slic3rPE",                   "slic3r.ini",              SLIC_SLIC3R},
    {"\\Slic3r",                     "slic3r.ini",              SLIC_SLIC3R}
};

// Command sets (a sprintf string that takes the bed centre as two floats)
// Indexed by slicer type
char slicer_cmd[MAX_TYPES][80] =
{
    " --print-center %d,%d ",                       // For vanilla slic3r and slic3rPE
    " -g --center %d,%d "                           // For PrusaSlicer
};


// Arrays of exe and config names, and the index to the current one of each.
SlicerExe slicer_exe[MAX_SLICERS];
SlicerConfig slicer_config[MAX_SLICERS];
int slicer_index = 0;
int config_index = 0;
int num_slicers = 0;
int num_configs = 0;

// Currently selected printer model.
char current_model[64];

// Section names in PrusaResearch.ini
char sect_names[MAX_SECT_NAME_SIZE];

// Cache of inheritable sections already loaded.
int n_cache = 0;
InhSection* cache[MAX_SECTIONS];

// Vendor name
#define VENDOR "PrusaResearch"

// Vendor file (currently the only one known and supported)
#define VENDOR_INI_FILE "\\vendor\\PrusaResearch.ini"


// Preferences for printer connection

// What sort of connection
BOOL print_octo = FALSE;                // TRUE for Octoprint, FALSE for serial

// Currently selected printer serial port (without colon: "COM10")
char printer_port[64] = "\0";

// If true, serial connections will send and check line numbers
BOOL print_serial_lineno = FALSE;

// The serial connection's baud rate. Zero if it has not been tested for yet
int print_serial_baud = 0;

// Octoprint server name (e.g. "octopi.local")
char octoprint_server[128] = "\0";

// Octoprint server API key
char octoprint_apikey[128] = "\0";

// If TRUE, user is prompted for a G-code filename. Otherwise, the slicer generates one.
BOOL explicit_gcode = TRUE;

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

    slicer_exe[0].exe[0] = '\0';
    slicer_config[0].dir[0] = '\0';

    for (i = 0; i < MAX_SLICERS; i++)
    {
        sprintf_s(location, 32, "Location%d", i);
        len = MAX_PATH;
        if (RegQueryValueEx(hkey, location, 0, NULL, str, &len) != ERROR_SUCCESS)
            break;
        rc = TRUE;
        strcpy_s(slicer_exe[i].exe, MAX_PATH, str);

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
        strcpy_s(slicer_config[i].dir, MAX_PATH, str);

        sprintf_s(location, 32, "ConfigIni%d", i);
        len = MAX_PATH;
        if (RegQueryValueEx(hkey, location, 0, NULL, str, &len) != ERROR_SUCCESS)
            break;
        strcpy_s(slicer_config[i].ini, MAX_PATH, str);

        sprintf_s(location, 32, "ConfigType%d", i);
        len = 4;
        if (RegQueryValueEx(hkey, location, 0, NULL, (LPBYTE)&slicer_config[i].type, &len) != ERROR_SUCCESS)
            break;

        num_configs++;
    }

    // Read the indices (they are DWORDs rather than strings)
    len = 4;
    RegQueryValueEx(hkey, "SlicerIndex", 0, NULL, (LPBYTE)&slicer_index, &len);
    len = 4;
    RegQueryValueEx(hkey, "ConfigIndex", 0, NULL, (LPBYTE)&config_index, &len);

    // Printer connection
    len = 4;
    RegQueryValueEx(hkey, "PrintOcto", 0, NULL, (LPBYTE)&print_octo, &len);
    len = 64;
    RegQueryValueEx(hkey, "PrinterPort", 0, NULL, (LPBYTE)printer_port, &len);
    len = 4;
    RegQueryValueEx(hkey, "PrintSerialBaud", 0, NULL, (LPBYTE)&print_serial_baud, &len);
    len = 128;
    RegQueryValueEx(hkey, "OctoServer", 0, NULL, (LPBYTE)octoprint_server, &len);
    len = 128;
    RegQueryValueEx(hkey, "OctoAPIKey", 0, NULL, (LPBYTE)octoprint_apikey, &len);

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
        RegSetValueEx(hkey, location, 0, REG_SZ, slicer_exe[i].exe, strlen(slicer_exe[i].exe) + 1);
    }
    for (; i < MAX_SLICERS; i++)        // clear out any old ones beyond the num
    {
        sprintf_s(location, 32, "Location%d", i);
        RegDeleteKeyValue(hkey, NULL, location);
    }

    for (i = 0; i < num_configs; i++)
    {
        sprintf_s(location, 32, "ConfigDir%d", i);
        RegSetValueEx(hkey, location, 0, REG_SZ, slicer_config[i].dir, strlen(slicer_config[i].dir) + 1);
        sprintf_s(location, 32, "ConfigIni%d", i);
        RegSetValueEx(hkey, location, 0, REG_SZ, slicer_config[i].ini, strlen(slicer_config[i].ini) + 1);
        sprintf_s(location, 32, "ConfigType%d", i);
        RegSetValueEx(hkey, location, 0, REG_DWORD, (PBYTE)&slicer_config[i].type, 4);
    }
    for (; i < MAX_SLICERS; i++)        // clear out any old ones beyond the num
    {
        sprintf_s(location, 32, "ConfigDir%d", i);
        RegDeleteKeyValue(hkey, NULL, location);
        sprintf_s(location, 32, "ConfigIni%d", i);
        RegDeleteKeyValue(hkey, NULL, location);
        sprintf_s(location, 32, "ConfigType%d", i);
        RegDeleteKeyValue(hkey, NULL, location);
    }

    // Save the slicer index and config index.
    RegSetValueEx(hkey, "SlicerIndex", 0, REG_DWORD, (LPBYTE)&slicer_index, 4);
    RegSetValueEx(hkey, "ConfigIndex", 0, REG_DWORD, (LPBYTE)&config_index, 4);

    // Printer connection
    RegSetValueEx(hkey, "PrintOcto", 0, REG_DWORD, (LPBYTE)&print_octo, 4);
    RegSetValueEx(hkey, "PrinterPort", 0, REG_SZ, (PBYTE)printer_port, strlen(printer_port) + 1);
    RegSetValueEx(hkey, "PrintSerialBaud", 0, REG_DWORD, (LPBYTE)&print_serial_baud, 4);
    RegSetValueEx(hkey, "OctoServer", 0, REG_SZ, (PBYTE)octoprint_server, strlen(octoprint_server) + 1);
    RegSetValueEx(hkey, "OctoAPIKey", 0, REG_SZ, (PBYTE)octoprint_apikey, strlen(octoprint_apikey) + 1);

    RegCloseKey(hkey);

    // Clear the section cache, in case slicer has changed.
    // TODO: Free the cached entries, or else re-use them (they are all the same size)
    n_cache = 0;
}


// Save the printer configuration.
void
save_printer_config()
{
    HKEY hkey;

    RegCreateKeyEx(HKEY_CURRENT_USER, "Software\\LoftyCAD\\Slic3r", 0, NULL, 0, KEY_ALL_ACCESS, NULL, &hkey, NULL);

    RegSetValueEx(hkey, "PrintOcto", 0, REG_DWORD, (LPBYTE)&print_octo, 4);
    RegSetValueEx(hkey, "PrinterPort", 0, REG_SZ, (PBYTE)printer_port, strlen(printer_port) + 1);
    RegSetValueEx(hkey, "PrintSerialBaud", 0, REG_DWORD, (LPBYTE)&print_serial_baud, 4);
    RegSetValueEx(hkey, "OctoServer", 0, REG_SZ, (PBYTE)octoprint_server, strlen(octoprint_server) + 1);
    RegSetValueEx(hkey, "OctoAPIKey", 0, REG_SZ, (PBYTE)octoprint_apikey, strlen(octoprint_apikey) + 1);

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
        strcat_s(location, MAX_PATH, slic3r_locations[i].directory);
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
            strcat_s(filename, MAX_PATH, "\\");
            strcat_s(filename, MAX_PATH, slic3r_locations[i].target);
            f = CreateFile(filename, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
            if (f != INVALID_HANDLE_VALUE)
            {
                // Found one - store it away filename and all.
                strcpy_s(slicer_exe[num_slicers++].exe, MAX_PATH, filename);
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
        strcat_s(location, MAX_PATH, config_locations[i].directory);

        // slic3r.ini must be present.
        strcpy_s(filename, MAX_PATH, location);
        strcat_s(filename, MAX_PATH, "\\");
        strcat_s(filename, MAX_PATH, config_locations[i].target);
        f = CreateFile(filename, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        if (f != INVALID_HANDLE_VALUE)
        {
            // Found it. The directory and target ini file are stored separately.
            slicer_config[num_configs].type = config_locations[i].type;
            strcpy_s(slicer_config[num_configs].ini, MAX_PATH, config_locations[i].target);
            strcpy_s(slicer_config[num_configs++].dir, MAX_PATH, location);
            CloseHandle(f);
        }
    }

    // Put the stuff we have found into the registry. Assume the selected index is 0 for now.
    save_slic3r_exe_and_config();

    return rc;
}

// Helper to find a key in an InhSection and return its value as a pointer.
char *
find_string_in_section(char* key, InhSection* s)
{
    int i;
    char* e;
    char ckey[SECT_NAME_SIZE];

    for (i = 0; i < s->n_keyvals; i++)
    {
        if (s->keyval[i].override)
            continue;
        e = strchr(s->keyval[i].key, '=');
        if (e == NULL)
            continue;
        strncpy_s(ckey, SECT_NAME_SIZE, s->keyval[i].key, e - s->keyval[i].key);
        if (strcmp(key, ckey) == 0)
            return s->keyval[i].value;
    }
    return NULL;
}

// Helper to test keys match. The actual strings are key=value, but we test up to the '='.
// We are not allowed to poke '\0' in.
BOOL
keys_match(char* kv1, char* kv2)
{
    char* e1 = strchr(kv1, '=');
    char* e2 = strchr(kv2, '=');

    if (e1 == NULL || e2 == NULL)   // Check no '=' signs
        return FALSE;
    if (e1 - kv1 != e2 - kv2)       // Check lengths match
        return FALSE;
    return strncmp(kv1, kv2, e1 - kv1) == 0;
}

// Pull apart the long string returned by GetPrivateProfileSection on "key:section"
// Returns pointers to NULL-separated key = value pairs, as well as pointers to the values (after '=')
// in an InhSection structure allocated here.
InhSection *
load_section(char *ini, char *key, char* sect)
{
    char* p;
    char* v;
    int keyval_len, len;
    int i, j;
    int n = 0;
    InhSection* s;
    char section[SECT_NAME_SIZE];
#ifdef DEBUG_WRITE_INI_SECTION
    char dbgini[MAX_PATH];
    FILE* f;
#endif // DEBUG_WRITE_INI_SECTION

    // Section is key:sect
    strcpy_s(section, SECT_NAME_SIZE, key);
    strcat_s(section, SECT_NAME_SIZE, ":");
    strcat_s(section, SECT_NAME_SIZE, sect);

    // See if the section has already been loaded.
    for (i = 0; i < n_cache; i++)
    {
        if (strcmp(section, cache[i]->sect_name) == 0)
            return cache[i];
    }

#ifdef DEBUG_WRITE_INI_SECTION
    // Get rid of * from sect
    strcpy_s(dbgini, MAX_PATH, key);
    strcat_s(dbgini, MAX_PATH, "_");
    strcat_s(dbgini, MAX_PATH, sect);
    for (p = dbgini; *p != '\0'; p++)
    {
        if (*p == '*')
            *p = '_';
    }
    fopen_s(&f, dbgini, "wt");
#endif // DEBUG_WRITE_INI_SECTION

    // No, make a new one and eventually put it in the cache.
    s = calloc(sizeof(InhSection), 1);
    strcpy_s(s->sect_name, 80, section);
    len = GetPrivateProfileSection(section, s->section_string, MAX_SECT_SIZE, ini);

    // section: key = value\0key = value\0 ... \0\0

    for (p = s->section_string; *p != '\0'; p += keyval_len + 1)
    {
        keyval_len = strlen(p);
        s->keyval[n].key = p;
        v = strchr(p, '=');
        if (v == NULL)
            s->keyval[n].value = NULL;       // no '=' found. Should never happen.
        else
        {
            do { v++; } while (*v == ' ');  // skip blanks
            s->keyval[n].value = v;
        }

        // Follow inheritance links. This keyword always occurs first.
        // It may take several sect names separated by ';'.
        if (strncmp(s->keyval[n].key, "inherits", 8) == 0)
        {
            char* ctxt = NULL;
            char* inh = strtok_s(s->keyval[n].value, ";", &ctxt);

            while (inh != NULL)
            {
                InhSection* inhs = load_section(ini, key, inh);

                // Copy the new section's key/value pairs into the current one,
                // checking for overrides if there is more than one inherited section.
                for (i = 0; i < inhs->n_keyvals; i++)
                {
                    // Copy the pointers. They point to the string in the other InhSection,
                    // but that's OK, as we will never free it anyway (it will sit in the cache).
                    // This copy may be set to override but the original must be left alone.
                    s->keyval[n] = inhs->keyval[i];

                    // Check if it conflicts with any existing ones.
                    // Warning: potential n-squared loop!
                    for (j = 0; j < n; j++)
                    {
                        if (keys_match(s->keyval[j].key, s->keyval[n].key))
                            s->keyval[j].override = TRUE;
                    }
                    n++;
                }

                // Copy any bed size inherited from the section we just read
                s->bed_xmin = inhs->bed_xmin;
                s->bed_xmax = inhs->bed_xmax;
                s->bed_ymin = inhs->bed_ymin;
                s->bed_ymax = inhs->bed_ymax;

                // See if there are any more sections to be inherited from.
                inh = strtok_s(NULL, ";", &ctxt);
            }
        }
        else
        {
            // Check if the key is overriding an earlier one.
            // Warning: potential n-squared loop!
            for (j = 0; j < n; j++)
            {
                if (keys_match(s->keyval[j].key, s->keyval[n].key))
                    s->keyval[j].override = TRUE;
            }

            // Extract bed min/max from the section if it exists.
            if (strncmp(s->keyval[n].key, "bed_shape", 9) == 0)
            {
                char* nexttok = NULL;
                char* tok;
                char bed_str[256];

                strcpy_s(bed_str, 256, s->keyval[n].value);
                tok = strtok_s(bed_str, "x, \t\n", &nexttok);      // 0x0
                s->bed_xmin = (float)atof(tok);
                tok = strtok_s(NULL, "x, \t\n", &nexttok);
                s->bed_ymin = (float)atof(tok);

                tok = strtok_s(NULL, ", \t\n", &nexttok);       // 250x0 (skip)
                tok = strtok_s(NULL, "x, \t\n", &nexttok);      // 250x210
                s->bed_xmax = (float)atof(tok);
                tok = strtok_s(NULL, "x, \t\n", &nexttok);
                s->bed_ymax = (float)atof(tok);
            }

            n++;
        }
    }
    s->n_keyvals = n;

#ifdef DEBUG_WRITE_INI_SECTION
    for (i = 0; i < s->n_keyvals; i++)
        fprintf(f, "%s%s\n", s->keyval[i].override ? "## " : "", s->keyval[i].key);
    fclose(f);
#endif // DEBUG_WRITE_INI_SECTION

    if (n_cache < MAX_SECTIONS - 1)
        cache[n_cache++] = s;

    return s;
}

// Set the bed shape for a printer
void
set_bed_shape(char* printer)
{
    InhSection* s;
    char vendor[MAX_PATH];
    
    strcpy_s(vendor, MAX_PATH, slicer_config[config_index].dir);
    strcat_s(vendor, MAX_PATH, VENDOR_INI_FILE);
    s = load_section(vendor, "printer", printer);

    if (s->bed_xmax > s->bed_xmin)
    {
        bed_xmin = s->bed_xmin;
        bed_xmax = s->bed_xmax;
        bed_ymin = s->bed_ymin;
        bed_ymax = s->bed_ymax;
    }
}

// Load a string from a section and put its value into the corresponding dialog item.
BOOL
read_string_and_load_dlgitem(char* sect, char* string, int dlg_item, BOOL checkbox)
{
    InhSection* s;
    char* val;
    char vendor[MAX_PATH];

    strcpy_s(vendor, MAX_PATH, slicer_config[config_index].dir);
    strcat_s(vendor, MAX_PATH, VENDOR_INI_FILE);
    s = load_section(vendor, "print", sect);
    val = find_string_in_section(string, s);
    if (val == NULL)
        return FALSE;

    // Set up the value in the edit box
    if (checkbox)  // string is "0" or "1"
        CheckDlgButton(hWndSlicer, dlg_item, string[0] == '1' ? BST_CHECKED : BST_UNCHECKED);
    else
        SendDlgItemMessage(hWndSlicer, dlg_item, WM_SETTEXT, 0, (LPARAM)val);

    // Mark the edit box as having an unchanged value
    SetWindowLong(GetDlgItem(hWndSlicer, dlg_item), GWL_USERDATA, 0);

    return TRUE;
}

// Read Slic3r configuration and populate allowable printers, print settings and filament settings
// according to the given key (printer, print or filament). The settings are loaded into the
// given combo box item in hwndSlicer.
// If a [vendor] section exists in slic3r.ini (e.g. PrusaResearch.ini) go from those model(s)
// and establish the applicable settings. For vanilla Slic3r, just enumerate the user preset ini files.
// Sel_printer is written for printer model. (note: not the same as the displayed name)
void
read_slic3r_config(char* key, int dlg_item, char *sel_printer)
{
    char dir[MAX_PATH], ini[MAX_PATH], vendor[MAX_PATH], filename[MAX_PATH];
    InhSection* vs, *s;
    char name[64];
    char buf[256];
    char* ctxt = NULL;
    HANDLE h;
    FILE* f;
    WIN32_FIND_DATA find_data;
    int len, i, n;
    char* p, *v, *model;
    char key_colon[64];
    int klen;
    char model_str[SECT_NAME_SIZE];
    char bed_str[256];
    char* e;

    // Clear the combo
    SendDlgItemMessage(hWndSlicer, dlg_item, CB_RESETCONTENT, 0, 0);

    // Look for user preset(s) in complete files, of the form <ConfigDir>\<key>\<preset>.ini
    strcpy_s(dir, MAX_PATH, slicer_config[config_index].dir);
    strcat_s(dir, MAX_PATH, "\\");
    strcat_s(dir, MAX_PATH, key);
    strcpy_s(filename, MAX_PATH, dir);
    strcat_s(filename, MAX_PATH, "\\*.ini");
    h = FindFirstFile(filename, &find_data);
    if (h != INVALID_HANDLE_VALUE)
    {
        while (1)
        {
            strcpy_s(filename, MAX_PATH, find_data.cFileName);
            len = strlen(filename);
            filename[len - 4] = '\0';    // strip ".ini"
            SendDlgItemMessage(hWndSlicer, dlg_item, CB_ADDSTRING, 0, (LPARAM)filename);

            // Create a InhSection struct for this section so it can be loaded later
            s = calloc(sizeof(InhSection), 1);

            // Section name is key:sect
            strcpy_s(s->sect_name, SECT_NAME_SIZE, key);
            strcat_s(s->sect_name, SECT_NAME_SIZE, ":");
            strcat_s(s->sect_name, SECT_NAME_SIZE, filename);

            // Open file and read the key=value pairs
            strcpy_s(filename, MAX_PATH, dir);
            strcat_s(filename, MAX_PATH, "\\");
            strcat_s(filename, MAX_PATH, find_data.cFileName);
            fopen_s(&f, filename, "rt");
            if (f != NULL)
            {
                // Pack all the key=value pairs into a long string
                p = s->section_string;
                n = 0;
                while (1)
                {
                    if (fgets(p, 2048, f) == NULL)      // these can be very long
                        break;
                    len = strlen(p);
                    s->keyval[n].key = p;
                    v = strchr(p, '=');
                    if (v == NULL)
                        s->keyval[n].value = NULL;       // no '=' found. Should never happen.
                    else
                    {
                        do { v++; } while (*v == ' ');  // skip blanks
                        s->keyval[n].value = v;
                    }

                    // Extract bed min/max from the section if it exists.
                    if (strncmp(s->keyval[n].key, "bed_shape", 9) == 0)
                    {
                        char* nexttok = NULL;
                        char* tok;

                        strcpy_s(bed_str, 256, s->keyval[n].value);        
                        tok = strtok_s(bed_str, "x, \t\n", &nexttok);      // 0x0
                        s->bed_xmin = (float)atof(tok);
                        tok = strtok_s(NULL, "x, \t\n", &nexttok);
                        s->bed_ymin = (float)atof(tok);

                        tok = strtok_s(NULL, ", \t\n", &nexttok);       // 250x0 (skip)
                        tok = strtok_s(NULL, "x, \t\n", &nexttok);      // 250x210
                        s->bed_xmax = (float)atof(tok);
                        tok = strtok_s(NULL, "x, \t\n", &nexttok);
                        s->bed_ymax = (float)atof(tok);
                    }

                    n++;
                    p += len;
                    *(p - 1) = '\0';    // overwrite the '\n' left by fgets
                }
                fclose(f);
                s->n_keyvals = n;

                // Cache the section
                if (n_cache < MAX_SECTIONS - 1)
                    cache[n_cache++] = s;
            }

            // Go back for more files
            if (!FindNextFile(h, &find_data))
                break;
        }
        FindClose(h);
    }

    // Find slic3r.ini (or PrusaSlicer.ini, or whatever it is)
    strcpy_s(dir, MAX_PATH, slicer_config[config_index].dir);
    strcpy_s(ini, MAX_PATH, dir);
    strcat_s(ini, MAX_PATH, "\\");
    strcat_s(ini, MAX_PATH, slicer_config[config_index].ini);

    // Find possible vendor file
    strcpy_s(vendor, MAX_PATH, slicer_config[config_index].dir);
    strcat_s(vendor, MAX_PATH, VENDOR_INI_FILE);

    // Look for printer, print or filament presets. Printer must be called first.
    if (strcmp(key, "printer") == 0)
    {
        // Read slic3r.ini and get the slicer name: # generated by XXXX on ....
        fopen_s(&f, ini, "rt");
        if (f == NULL)
            return;     // no slic3r.ini, can't do much more
        fgets(buf, 256, f);
        if (buf[0] == '#')  // must be a comment
        {
            char* by = strstr(buf, " by ");
            char* on = strstr(buf, " on ");

            if (by != NULL && on != NULL)
            {
                *on = '\0';
                SendDlgItemMessage(hWndSlicer, IDC_STATIC_SLICER, WM_SETTEXT, 0, (LPARAM)(by + 4));
            }
            fclose(f);
        }
                        
        // Look for [vendor:PrusaResearch] in slic3r.ini to find all the installed printer models
        vs = load_section(ini, "vendor", VENDOR);
        if (vs->n_keyvals != 0)
        {
            // Get the names of all the sections so we can later filter them out by key (print or filament)
            GetPrivateProfileSectionNames(sect_names, MAX_SECT_NAME_SIZE, vendor);
        }
        else
        {
            // There's no vendor file, so clear out the list
            sect_names[0] = '\0';
        }

        // Filter out the section names by key (printer:xxxx)
        strcpy_s(key_colon, 64, key);
        strcat_s(key_colon, 64, ":");
        klen = strlen(key_colon);

        for (p = sect_names; *p != '\0'; p += len + 1)
        {
            len = strlen(p);

            // key:name --> name, and skip the ones with '*' at front
            if (strncmp(key_colon, p, klen) == 0 && *(p + klen) != '*')
            {
                // Load the section so we can get the printer_model string (it may be buried
                // in an inherited section)
                s = load_section(vendor, key, p + klen);

                // Check printer_model against vs
                model = find_string_in_section("printer_model", s);
                for (i = 0; i < vs->n_keyvals; i++)
                {
                    // We just want the model:xxxx key (not its value).
                    strcpy_s(model_str, SECT_NAME_SIZE, vs->keyval[i].key);
                    if ((e = strchr(model_str, '=')) != NULL)
                        *e = '\0';
                    if (strcmp(&model_str[6], model) == 0)      // skip "model:"
                    {
                        // Add the name to the list
                        SendDlgItemMessage(hWndSlicer, dlg_item, CB_ADDSTRING, 0, (LPARAM)(p + klen));
                        break;
                    }
                }
            }
        }

        // Select the preset given in slic3r.ini (the last one worked on in the Slic3r GUI)
        GetPrivateProfileString("presets", key, "", name, 64, ini);
        i = SendDlgItemMessage(hWndSlicer, dlg_item, CB_FINDSTRINGEXACT, -1, (LPARAM)name);
        sel_printer[0] = '\0';
        if (i != CB_ERR)
        {
            SendDlgItemMessage(hWndSlicer, dlg_item, CB_SETCURSEL, i, 0);

            // Return the selected printer name
            strcpy_s(sel_printer, 64, name);

            // Set the bed shape
            set_bed_shape(name);
        }
    }
    else   // print or filament.
    {
        // Build a PRINTER_MODEL_XXX string for the selected printer
        // model will be NULL if there is no vendor file.
        s = load_section(vendor, "printer", sel_printer);
        model = find_string_in_section("printer_model", s);
        strcpy_s(model_str, SECT_NAME_SIZE, "PRINTER_MODEL_");
        if (model != NULL)
            strcat_s(model_str, SECT_NAME_SIZE, model);

        // Filter out the section names by key (print:xxxx or filament:xxxx)
        strcpy_s(key_colon, 64, key);
        strcat_s(key_colon, 64, ":");
        klen = strlen(key_colon);

        for (p = sect_names; *p != '\0'; p += len + 1)
        {
            len = strlen(p);

            // key:name --> name, and skip the ones with '*' at front
            if (strncmp(key_colon, p, klen) == 0 && *(p + klen) != '*')
            {
                char compat[1024];

                // Check compatibility only for print (not filament).
                // Don't load the section yet (too many unused sections will be loaded).
                // Instead, just find the compat string which is assumed to be at top level
                // and do a quick and dirty substring match for the PRINTER_MODEL_XXX string.
                if (strcmp(key, "print") == 0 && model != NULL)
                {
                    GetPrivateProfileString(p, "compatible_printers_condition", "", compat, 1024, vendor);
                    if (strstr(compat, model_str) == NULL)
                        continue;
                }

                // Add the name to the list
                SendDlgItemMessage(hWndSlicer, dlg_item, CB_ADDSTRING, 0, (LPARAM)(p + klen));
            }
        }

        // Select the preset given in slic3r.ini (the last one worked on in the Slic3r GUI)
        GetPrivateProfileString("presets", key, "", name, 64, ini);
        i = SendDlgItemMessage(hWndSlicer, dlg_item, CB_FINDSTRINGEXACT, -1, (LPARAM)name);
        if (i != CB_ERR)
            SendDlgItemMessage(hWndSlicer, dlg_item, CB_SETCURSEL, i, 0);
    }
}

// Build one of a set of ini files to load to Slic3r command line.
// Given a preset key (printer, print or filament) as found in [presets] in slic3r.ini :
// - find any user-created settings under the config directory and return those
// - if not found, find the section in the [vendor] ini file (if any), following chains of 
//   inheritance and overriding individual settings as needed.
// Write to the given ini file to be given to the slic3r command.
// FALSE is returned if nothing was found.
BOOL
get_slic3r_config_section(char* key, char* preset, char *inifile)
{
    InhSection* s;
    FILE* ini;
    char vendor[MAX_PATH];
    int i;

    // Find possible vendor file and load the section.
    // (User preset sections will always be in the cache, and will ignore the filename)
    strcpy_s(vendor, MAX_PATH, slicer_config[config_index].dir);
    strcat_s(vendor, MAX_PATH, VENDOR_INI_FILE);
    s = load_section(vendor, key, preset);
    if (s == NULL || s->n_keyvals == 0)
        return FALSE;

    fopen_s(&ini, inifile, "wt");
    if (ini == NULL)
        return FALSE;

    fprintf_s(ini, "# %s:%s\n", key, preset);
    for (i = 0; i < s->n_keyvals; i++)
    {
        // Clean out comments and overridden settings
        if (!isalpha(s->keyval[i].key[0]))
            continue;
        if (s->keyval[i].override)
            continue;
        fprintf_s(ini, "%s\n", s->keyval[i].key);
    }
    fclose(ini);
    return TRUE;
}

#define BUFSIZE 4096
#define PATH_STR "Using an 8.3 path"
#define OUTPUT_FILE_STR "Exporting G-code to "

// Run the slicer and capture its output. (Courtesy of MS process-read-from-pipe example)
BOOL
run_slicer(char* slicer_exe, char* cmd_line, char* dir, char* gcode_filename)
{
    HANDLE g_hChildStd_OUT_Rd = NULL;
    HANDLE g_hChildStd_OUT_Wr = NULL;

    SECURITY_ATTRIBUTES saAttr;
    PROCESS_INFORMATION piProcInfo;
    STARTUPINFO siStartInfo;
    BOOL bSuccess = FALSE;
    DWORD dwRead;
    CHAR chBuf[BUFSIZE];
    char line[256], out_filename[MAX_PATH];
    char* bufptr, * bufend, *endline;
    BOOL have_output = FALSE;

    // Set the bInheritHandle flag so pipe handles are inherited. 
    saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
    saAttr.bInheritHandle = TRUE;
    saAttr.lpSecurityDescriptor = NULL;

    // Create a pipe for the child process's STDOUT. 
    if (!CreatePipe(&g_hChildStd_OUT_Rd, &g_hChildStd_OUT_Wr, &saAttr, 0))
        return FALSE;

    // Ensure the read handle to the pipe for STDOUT is not inherited.
    if (!SetHandleInformation(g_hChildStd_OUT_Rd, HANDLE_FLAG_INHERIT, 0))
        return FALSE;

    // Set up members of the PROCESS_INFORMATION structure. 
    ZeroMemory(&piProcInfo, sizeof(PROCESS_INFORMATION));

    // Set up members of the STARTUPINFO structure. 
    // This structure specifies the STDIN and STDOUT handles for redirection.
    // We only need stdout/err.
    // Keep console window hidden (progress and status will show instead)
    ZeroMemory(&siStartInfo, sizeof(STARTUPINFO));
    siStartInfo.cb = sizeof(STARTUPINFO);
    siStartInfo.hStdError = g_hChildStd_OUT_Wr;
    siStartInfo.hStdOutput = g_hChildStd_OUT_Wr;
    siStartInfo.hStdInput = NULL;
    siStartInfo.wShowWindow = SW_HIDE;
    siStartInfo.dwFlags |= STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;

    // Create the child process. Log stuff.
    Log(slicer_exe);
    Log("\r\n");
    Log(cmd_line);
    Log("\r\n");
    bSuccess = CreateProcess
    (
        slicer_exe,    // application name
        cmd_line,      // command line 
        NULL,          // process security attributes 
        NULL,          // primary thread security attributes 
        TRUE,          // handles are inherited 
        0,             // creation flags 
        NULL,          // use parent's environment 
        dir,           // current directory 
        &siStartInfo,  // STARTUPINFO pointer 
        &piProcInfo);  // receives PROCESS_INFORMATION 

     // If an error occurs, bail here. 
    if (!bSuccess)
        return FALSE;

    // Close handles to the child process and its primary thread.
    CloseHandle(piProcInfo.hProcess);
    CloseHandle(piProcInfo.hThread);

    // Close handles to the stdout pipes no longer needed by the child process.
    CloseHandle(g_hChildStd_OUT_Wr);

    // Read from pipe until process finishes. Accumulate characters in a buffer and
    // log whole lines out up to the last \r\n. Look for the output filename as we go.
    bufptr = bufend = chBuf;
    set_progress_range(10);
    for (;;)
    {
        char* p;

        // Read a small amount at a time (16 chars)
        bSuccess = ReadFile(g_hChildStd_OUT_Rd, bufend, 16, &dwRead, NULL);
        if (!bSuccess || dwRead == 0) 
            break;
        bufend += dwRead;
        *bufend = '\0';
        endline = strrchr(bufptr, '\n');
        if (endline != NULL)
        {
            // Copy the whole line(s) so far, to avoid having to plant \0 in the chBuf
            endline++;
            strncpy_s(line, 256, bufptr, endline - bufptr);
            bufptr = endline;

            // Skip the line that talks about the 8.3 path, as it is not useful
            if (strstr(line, PATH_STR) == NULL)
            {
                Log(line);
                bump_progress();
            }

            // Check for output filename
            if (!have_output && (p = strstr(line, OUTPUT_FILE_STR)) != NULL)
            {
                if (explicit_gcode)
                {
                    strcpy_s(out_filename, MAX_PATH, gcode_filename);
                }
                else
                {
                    p += strlen(OUTPUT_FILE_STR);
                    endline = strchr(p, '\r');
                    *endline = '\0';
                    strcpy_s(out_filename, MAX_PATH, p);
                }
                have_output = TRUE;
            }
        }
    }
    Log("\r\n");
    clear_status_and_progress();

    // If we have an output, send it to the print preview. Otherwise, alert via a message box.
    if (have_output)
    {
        purge_zpoly_edges(&gcode_tree);
        if (!read_gcode_to_group(&gcode_tree, out_filename))
        {
            MessageBox(hWndSlicer, "Slicer output was not found", out_filename, MB_OK | MB_ICONWARNING);
            return FALSE;
        }
        invalidate_dl();
        SendMessage(hWndPropSheet, PSM_SETCURSEL, TAB_PREVIEW, 0);  // select print preview tab
        SendDlgItemMessage(hWndPrintPreview, IDC_PRINT_FILENAME, WM_SETTEXT, 0, (LPARAM)out_filename);
        SendDlgItemMessage(hWndPrintPreview, IDC_PRINT_FIL_USED, WM_SETTEXT, 0, (LPARAM)gcode_tree.fil_used);
        SendDlgItemMessage(hWndPrintPreview, IDC_PRINT_EST_PRINT, WM_SETTEXT, 0, (LPARAM)gcode_tree.est_print);
        EnableWindow(GetDlgItem(hWndPrintPreview, IDB_PRINTER_PRINT), TRUE);
    }
    else
    {
        MessageBox(hWndSlicer, chBuf, "Slicer failure", MB_OK | MB_ICONWARNING);
    }

    return TRUE;
}
