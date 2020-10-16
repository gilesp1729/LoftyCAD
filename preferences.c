#include "stdafx.h"
#include "LoftyCAD.h"
#include <CommCtrl.h>
#include <CommDlg.h>
#include <stdio.h>
#include <shellapi.h>
#include <setupapi.h>

// Preferences dialog and helpers.

// Routines to enumerate COM ports for the printer connection.
#define MAX_PORTS      8
#define MAX_NAME_PORTS 7
#define RegDisposition_OpenExisting (0x00000001) // open key only if exists
#define CM_REGISTRY_HARDWARE        (0x00000000)

typedef DWORD
(WINAPI *CM_Open_DevNode_Key)(DWORD, DWORD, DWORD, DWORD, PHKEY, DWORD);

HANDLE  BeginEnumeratePorts(VOID)
{
    BOOL guidTest = FALSE;
    DWORD RequiredSize = 0;
    HDEVINFO DeviceInfoSet;
    char* buf;

    guidTest = SetupDiClassGuidsFromNameA(
        "Ports", 0, 0, &RequiredSize);
    if (RequiredSize < 1)
        return INVALID_HANDLE_VALUE;

    buf = malloc(RequiredSize * sizeof(GUID));

    guidTest =
        SetupDiClassGuidsFromNameA("Ports", (GUID*)buf, RequiredSize * sizeof(GUID), &RequiredSize);

    if (!guidTest)
        return INVALID_HANDLE_VALUE;

    DeviceInfoSet = SetupDiGetClassDevs(
        (GUID*)buf, NULL, NULL, DIGCF_PRESENT);
    if (DeviceInfoSet == INVALID_HANDLE_VALUE)
        return INVALID_HANDLE_VALUE;

    free(buf);

    return DeviceInfoSet;
}

BOOL EnumeratePortsNext(HANDLE DeviceInfoSet, LPTSTR lpBuffer, int bufsize)
{
    static CM_Open_DevNode_Key OpenDevNodeKey = NULL;
    static HINSTANCE CfgMan;

    int res1;
    char DevName[MAX_NAME_PORTS] = { 0 };
    static int numDev = 0;
    int numport;

    SP_DEVINFO_DATA DeviceInfoData = { 0 };
    DeviceInfoData.cbSize = sizeof(SP_DEVINFO_DATA);
    if (!DeviceInfoSet || !lpBuffer)
        return -1;
    
    // First time in, find the CM_Open_DevNode_Key function in cfgmgr32.dll .
    if (!OpenDevNodeKey)
    {
        CfgMan = LoadLibrary("cfgmgr32");
        if (!CfgMan)
            return FALSE;
        OpenDevNodeKey = (CM_Open_DevNode_Key)GetProcAddress(CfgMan, "CM_Open_DevNode_Key");
        if (!OpenDevNodeKey)
        {
            FreeLibrary(CfgMan);
            return FALSE;
        }
    }

    while (TRUE)
    {

        HKEY KeyDevice;
        DWORD len;

        res1 = SetupDiEnumDeviceInfo(DeviceInfoSet, numDev, &DeviceInfoData);
        if (!res1)
        {
            SetupDiDestroyDeviceInfoList(DeviceInfoSet);
            FreeLibrary(CfgMan);
            OpenDevNodeKey = NULL;
            return FALSE;
        }

        // Open the device key.
        res1 = OpenDevNodeKey(DeviceInfoData.DevInst, KEY_QUERY_VALUE, 0,
            RegDisposition_OpenExisting, &KeyDevice, CM_REGISTRY_HARDWARE);
        if (res1 != ERROR_SUCCESS)
            return FALSE;

        len = MAX_NAME_PORTS;
        res1 = RegQueryValueEx
        (
            KeyDevice,    // handle of key to query
            "portname",    // address of name of value to query
            NULL,    // reserved
            NULL,    // address of buffer for value type
            DevName,    // address of data buffer
            &len     // address of data buffer size
        );

        RegCloseKey(KeyDevice);
        if (res1 != ERROR_SUCCESS)
            return FALSE;

        numDev++;
        if (_memicmp(DevName, "com", 3))
            continue;

        numport = atoi(DevName + 3);
        if (numport > 0 && numport <= 256)
        {
            // Found a COM port.
            strcpy_s(lpBuffer, bufsize, DevName);
            return TRUE;
        }

        FreeLibrary(CfgMan);
        OpenDevNodeKey = NULL;
        return FALSE;
    }
}

BOOL  EndEnumeratePorts(HANDLE DeviceInfoSet)
{
    return SetupDiDestroyDeviceInfoList(DeviceInfoSet);
}

// Preferences dialog.
int WINAPI
prefs_dialog(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    char buf[16], version[128];
    char location[MAX_PATH], filename[MAX_PATH];
    FILE* f;
    float new_val;
    int i;
    static BOOL slicer_changed, index_changed, config_changed;
    char printer[64];
    HANDLE dis;

    switch (msg)
    {
    case WM_INITDIALOG:
        SendDlgItemMessage(hWnd, IDC_PREFS_TITLE, WM_SETTEXT, 0, (LPARAM)object_tree.title);
        sprintf_s(buf, 16, "%.0f", half_size);
        SendDlgItemMessage(hWnd, IDC_PREFS_HALFSIZE, WM_SETTEXT, 0, (LPARAM)buf);
        sprintf_s(buf, 16, "%.2f", grid_snap);
        SendDlgItemMessage(hWnd, IDC_PREFS_GRID, WM_SETTEXT, 0, (LPARAM)buf);
        sprintf_s(buf, 16, "%.2f", tolerance);
        SendDlgItemMessage(hWnd, IDC_PREFS_TOL, WM_SETTEXT, 0, (LPARAM)buf);
        sprintf_s(buf, 16, "%d", angle_snap);
        SendDlgItemMessage(hWnd, IDC_PREFS_ANGLE, WM_SETTEXT, 0, (LPARAM)buf);
        sprintf_s(buf, 16, "%.2f", round_rad);
        SendDlgItemMessage(hWnd, IDC_PREFS_ROUNDRAD, WM_SETTEXT, 0, (LPARAM)buf);
        SetFocus(GetDlgItem(hWnd, IDC_PREFS_TITLE));

        CheckDlgButton(hWnd, IDC_PREFS_EXPLICIT_GCODE, explicit_gcode ? BST_CHECKED : BST_UNCHECKED);

        // Load up printer server selection (serial or Octoprint)


        // Load up serial ports.
        dis = BeginEnumeratePorts();
        while (EnumeratePortsNext(dis, buf, 16))
        {
            i = SendDlgItemMessage(hWnd, IDC_PREFS_SERIALPORT, CB_ADDSTRING, 0, (LPARAM)buf);
        }
        EndEnumeratePorts(dis);
        i = SendDlgItemMessage(hWndSlicer, IDC_PREFS_SERIALPORT, CB_FINDSTRINGEXACT, -1, (LPARAM)printer_port);
        if (i == CB_ERR)
            i = 0;
        SendDlgItemMessage(hWnd, IDC_PREFS_SERIALPORT, CB_SETCURSEL, i, 0);

        // Load up Octoprint info
        CheckDlgButton(hWnd, IDC_RADIO_SERIALPORT, print_octo ? BST_UNCHECKED : BST_CHECKED);
        CheckDlgButton(hWnd, IDC_RADIO_OCTOPRINT, print_octo ? BST_CHECKED : BST_UNCHECKED);
        EnableWindow(GetDlgItem(hWnd, IDC_PREFS_SERIALPORT), !print_octo);
        EnableWindow(GetDlgItem(hWnd, IDC_PREFS_OCTOPRINT), print_octo);
        EnableWindow(GetDlgItem(hWnd, IDC_PREFS_OCTO_APIKEY), print_octo);
        EnableWindow(GetDlgItem(hWnd, IDC_PREFS_OCTO_TEST), print_octo);
        SendDlgItemMessage(hWnd, IDC_PREFS_OCTOPRINT, WM_SETTEXT, 0, (LPARAM)octoprint_server);
        SendDlgItemMessage(hWnd, IDC_PREFS_OCTO_APIKEY, WM_SETTEXT, 0, (LPARAM)octoprint_apikey);
        SendDlgItemMessage(hWnd, IDC_STATIC_TEST_RESULT, WM_SETTEXT, 0, (LPARAM)"");

        // Load up slicer exe and config to combo boxes.
        index_changed = FALSE;
        slicer_changed = FALSE;
        config_changed = FALSE;

    load_combo:
        SendDlgItemMessage(hWnd, IDC_PREFS_SLICER_EXE, CB_RESETCONTENT, 0, 0);
        SendDlgItemMessage(hWnd, IDC_PREFS_SLICER_CONFIG, CB_RESETCONTENT, 0, 0);
        for (i = 0; i < num_slicers; i++)
            SendDlgItemMessage(hWnd, IDC_PREFS_SLICER_EXE, CB_INSERTSTRING, i, (LPARAM)slicer_exe[i].exe);
        for (i = 0; i < num_configs; i++)
            SendDlgItemMessage(hWnd, IDC_PREFS_SLICER_CONFIG, CB_INSERTSTRING, i, (LPARAM)slicer_config[i].dir);
        SendDlgItemMessage(hWnd, IDC_PREFS_SLICER_EXE, CB_SETCURSEL, slicer_index, 0);
        SendDlgItemMessage(hWnd, IDC_PREFS_SLICER_CONFIG, CB_SETCURSEL, config_index, 0);
        break;

    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        case IDOK:
            SendDlgItemMessage(hWnd, IDC_PREFS_TITLE, WM_GETTEXT, 256, (LPARAM)object_tree.title);

            SendDlgItemMessage(hWnd, IDC_PREFS_HALFSIZE, WM_GETTEXT, 16, (LPARAM)buf);
            half_size = (float)atof(buf);
            zTrans = -2.0f * half_size;
            Position(FALSE, 0, 0, 0, 0);

            SendDlgItemMessage(hWnd, IDC_PREFS_TOL, WM_GETTEXT, 16, (LPARAM)buf);
            new_val = (float)atof(buf);
            if (!nz(new_val - tolerance))
            {
                // The snapping tol and chamfer rad are fixed fractions of the tolerance. Don't change them.
                snap_tol = 3 * tolerance;
                chamfer_rad = 3.5f * tolerance;
                tol_log = (int)ceilf(log10f(1.0f / tolerance));

                // Fix up all the flattened curves.
                adjust_stepsizes((Object*)&object_tree, new_val);
                clear_move_copy_flags((Object*)&object_tree);
                invalidate_all_view_lists((Object*)&object_tree, (Object*)&object_tree, 0, 0, 0);

                tolerance = new_val;
                drawing_changed = TRUE;

                if (view_rendered)
                {
                    // regenerate surface mesh, in case we're viewing rendered
                    xform_list.head = NULL;
                    xform_list.tail = NULL;
                    if (object_tree.mesh != NULL)
                        mesh_destroy(object_tree.mesh);
                    object_tree.mesh = NULL;
                    object_tree.mesh_valid = FALSE;
                    gen_view_list_tree_volumes(&object_tree);
                    gen_view_list_tree_surfaces(&object_tree, &object_tree);
                }
            }

            // These don't change the drawing until something else is added.
            SendDlgItemMessage(hWnd, IDC_PREFS_GRID, WM_GETTEXT, 16, (LPARAM)buf);
            grid_snap = (float)atof(buf);
            SendDlgItemMessage(hWnd, IDC_PREFS_ANGLE, WM_GETTEXT, 16, (LPARAM)buf);
            angle_snap = atoi(buf);
            SendDlgItemMessage(hWnd, IDC_PREFS_ROUNDRAD, WM_GETTEXT, 16, (LPARAM)buf);
            round_rad = (float)atof(buf);

            // Store any change in the selected printer and its settings
            SendDlgItemMessage(hWnd, IDC_PREFS_SERIALPORT, WM_GETTEXT, 64, (LPARAM)printer_port);
            SendDlgItemMessage(hWnd, IDC_PREFS_OCTOPRINT, WM_GETTEXT, 128, (LPARAM)octoprint_server);
            SendDlgItemMessage(hWnd, IDC_PREFS_OCTO_APIKEY, WM_GETTEXT, 128, (LPARAM)octoprint_apikey);
            save_printer_config();

            // Slicer changes, in and of themselves, don't change the drawing. But save
            // any changes in the registry (even on cancel, as the internal lists have changed)
            explicit_gcode = IsDlgButtonChecked(hWnd, IDC_PREFS_EXPLICIT_GCODE);
            if (index_changed)
            {
                save_slic3r_exe_and_config();
                read_slic3r_config("printer", IDC_SLICER_PRINTER, printer);
                read_slic3r_config("print", IDC_SLICER_PRINTSETTINGS, printer);
                read_slic3r_config("filament", IDC_SLICER_FILAMENT, printer);
            }

            EndDialog(hWnd, drawing_changed);
            break;

        case IDCANCEL:
            if (index_changed)
            {
                save_slic3r_exe_and_config();
                read_slic3r_config("printer", IDC_SLICER_PRINTER, printer);
                read_slic3r_config("print", IDC_SLICER_PRINTSETTINGS, printer);
                read_slic3r_config("filament", IDC_SLICER_FILAMENT, printer);
            }

            EndDialog(hWnd, 0);
            break;

        case IDC_PREFS_FIND_SLICERS:
            find_slic3r_exe_and_config();
            index_changed = TRUE;
            goto load_combo;

        case IDC_PREFS_SLICER_EXE:
            switch (HIWORD(wParam))
            {
            case CBN_SELCHANGE:
                slicer_index = SendDlgItemMessage(hWnd, IDC_PREFS_SLICER_EXE, CB_GETCURSEL, 0, 0);
                index_changed = TRUE;
                break;

            case CBN_EDITCHANGE:
                slicer_changed = TRUE;
                break;

            case CBN_KILLFOCUS:
                if (slicer_changed)
                {
                    SendDlgItemMessage(hWnd, IDC_PREFS_SLICER_EXE, WM_GETTEXT, MAX_PATH, (LPARAM)location);
                    i = SendDlgItemMessage(hWnd, IDC_PREFS_SLICER_EXE, CB_ADDSTRING, 0, (LPARAM)location);
                    if (i >= MAX_SLICERS)
                        i = MAX_SLICERS - 1;
                    if (i >= num_slicers)
                        num_slicers = i + 1;
                    strcpy_s(slicer_exe[i].exe, MAX_PATH, location);
                    slicer_index = i;
                    index_changed = TRUE;
                    slicer_changed = FALSE;
                }
                break;
            }
            break;

        case IDC_PREFS_SLICER_CONFIG:
            switch (HIWORD(wParam))
            {
            case CBN_SELCHANGE:
                config_index = SendDlgItemMessage(hWnd, IDC_PREFS_SLICER_CONFIG, CB_GETCURSEL, 0, 0);
                index_changed = TRUE;
                break;

            case CBN_EDITCHANGE:
                config_changed = TRUE;
                break;

            case CBN_KILLFOCUS:
                if (config_changed)
                {
                    char inifiles[2][32] = { "slic3r.ini", "PrusaSlicer.ini" };
                    SLICER type;

                    // User has typed in a new location.
                    SendDlgItemMessage(hWnd, IDC_PREFS_SLICER_CONFIG, WM_GETTEXT, MAX_PATH, (LPARAM)location);

                    // Check location for the presence of an INI file (slic3r.ini or PrusaSlicer.ini)
                    // Remember its name, and what kind of slicer it is.
                    for (type = 0; type < MAX_TYPES; type++)
                    {
                        strcpy_s(filename, MAX_PATH, location);
                        strcat_s(filename, MAX_PATH, "\\");
                        strcat_s(filename, MAX_PATH, inifiles[type]);
                        f = CreateFile(filename, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
                        if (f != INVALID_HANDLE_VALUE)
                        {
                            CloseHandle(f);
                            break;
                        }
                    }
                    if (type == MAX_TYPES)
                        break;              // no INI file found

                    // Add the location to the list.
                    i = SendDlgItemMessage(hWnd, IDC_PREFS_SLICER_CONFIG, CB_ADDSTRING, 0, (LPARAM)location);
                    if (i >= MAX_SLICERS)
                        i = MAX_SLICERS - 1;
                    if (i >= num_configs)
                        num_configs = i + 1;
                    strcpy_s(slicer_config[i].dir, MAX_PATH, location);
                    strcpy_s(slicer_config[i].ini, MAX_PATH, inifiles[type]);
                    slicer_config[i].type = type;
                    config_index = i;
                    index_changed = TRUE;
                    config_changed = FALSE;
                }
                break;
            }
            break;

        case IDC_RADIO_SERIALPORT:
            print_octo = FALSE;
            goto enable_fields;

        case IDC_RADIO_OCTOPRINT:
            print_octo = TRUE;
        enable_fields:
            EnableWindow(GetDlgItem(hWnd, IDC_PREFS_SERIALPORT), !print_octo);
            EnableWindow(GetDlgItem(hWnd, IDC_PREFS_OCTOPRINT), print_octo);
            EnableWindow(GetDlgItem(hWnd, IDC_PREFS_OCTO_APIKEY), print_octo);
            EnableWindow(GetDlgItem(hWnd, IDC_PREFS_OCTO_TEST), print_octo);
            break;

        case IDC_PREFS_OCTO_TEST:
            SendDlgItemMessage(hWnd, IDC_STATIC_TEST_RESULT, WM_SETTEXT, 0, (LPARAM)"Attempting connection...");
            if (get_octo_version(version, 128))
                SendDlgItemMessage(hWnd, IDC_STATIC_TEST_RESULT, WM_SETTEXT, 0, (LPARAM)version);
            else
                SendDlgItemMessage(hWnd, IDC_STATIC_TEST_RESULT, WM_SETTEXT, 0, (LPARAM)"Cannot connect to server");
            break;
        }
    }

    return 0;
}


