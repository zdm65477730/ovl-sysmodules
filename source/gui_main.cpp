#include "gui_main.hpp"
#include "dir_iterator.hpp"

#include <SimpleIniParser.hpp>

/* Below copy from rexshao's source code
\uE093 ↓
\uE092 ↑
\uE091 ←
\uE090 →
\uE08f 逆时针转
\uE08e 顺时针转
\uE08d 上下箭头
\uE08c  左右箭头
\uE0Ed 方向键左
\uE0Ee 方向键 右
\uE0EB 方向键 上
\uE0EC  方向键 下
\uE0EA 方向键4个
\uE0Ef  ➕键 带白底
\uE0f0 ➖键 带白底
\uE0f1 ➕键
\uE0f2 ➖键
\uE0f3 电源键
\uE0f4  home 键 白底
\uE0f5  截屏键
特殊字符  \uE098 X
\uE099  像是切换视角按钮
特殊字符  \uE0E0  A按钮
特殊字符  \uE0E1  B按钮
特殊字符  \uE0E2  X按钮
特殊字符  \uE0E3  Y按钮BC
\uE0E4  L
\uE0E5  R
\uE0E6 ZL
\uE0E7 ZR
\uE0E8 SL
\uE0E9 SR
\uE150 ！圆底
\uE151 ！方底
\uE152 ❓圆底
\uE153 i圆白底
\uE14E 禁止图标
\uE14D i圆空底
\uE14c  ×
\uE14B  √
\uE14A >
\uE149 <
\uE148 上尖括号
\uE147 下尖括号
*/

using json = nlohmann::json;
using namespace tsl;

constexpr const char *const amsContentsPath = "/atmosphere/contents";
//constexpr const char *const boot2FlagFormat = "/atmosphere/contents/%016lX/flags/boot2.flag";
//constexpr const char *const boot2FlagFolder = "/atmosphere/contents/%016lX/flags";
constexpr const char *const sxosTitlesPath = "/sxos/titles";
constexpr const char *const boot2FlagFile = "/%016lX/flags/boot2.flag";
constexpr const char *const boot2FlagsFolder = "/%016lX/flags";
constexpr const char *const toolboxJsonPath = "/%.*s/toolbox.json";

static constexpr u32 AMSVersionConfigItem = 65000;
//static constexpr s64 SXOS_MIN_BOOT_SIZE = 10 * 1024 * 1024;

#if 0
/* Calibration
 * This is the raw data stored under the PRODINFO partition.
 * Offset	Size	Field                       escription
 * 0x0008	0x04	BodySize                    Total size of calibration data starting at offset 0x40.
 * 0x0020	0x20	BodyHash                    SHA256 hash calculated over calibration data.
 * 0x0088	0x180	WlanCountryCodes            Array of WLAN country code strings. Each element is 3 bytes (code + NULL terminator).
 */
static constexpr s64 BodySize_OFFSET = 0x0008;
static constexpr s64 BodyHash_OFFSET = 0x0020;
static constexpr s64 BodyHash_SIZE = 0x20;
static constexpr s64 BodyHashBeigin_OFFSET = 0x0040;
static constexpr s64 WlanCountryCodes_OFFSET = 0x0088;
static constexpr s64 WlanCountryCode_SIZE = 0x3;
static constexpr char CHINA_WLAN_COUNTRY_CODE[WlanCountryCode_SIZE] = "S4";
static constexpr char JAPAN_WLAN_COUNTRY_CODE[WlanCountryCode_SIZE] = "U1";
static constexpr char JAPAN_WLAN_COUNTRY_CODE1[WlanCountryCode_SIZE] = "R1";
#endif

static std::string boot2FlagFormat{amsContentsPath};
static std::string boot2FlagFolder{amsContentsPath};
static char pathBuffer[FS_MAX_PATH];

constexpr const char *const bootFiledescriptions[2] = {
        [0] = "SXOS boot.dat",
        [1] = "SXGEAR boot.dat"
};

GuiMain::GuiMain() {
    Result rc;

    static std::string jsonStr = R"(
        {
            "PluginName": "ovl-sysmodules",
            "RunAndHasFlagListItemValue": "In use | \uE0F4",
            "RunAndNoFlagListItemValue": "In use | \uE098",
            "NotRunAndHasFlagListItemValue": "Not in use | \uE0F4",
            "NotRunAndNoFlagListItemValue": "Not in use | \uE098",
            "PowerCategoryHeaderText": "SWITCH Power Control  |  \uE0E0  Restart and Power off",
            "PowerCustomDrawerText": "\uE016  Quick reset or power off your console.",
            "PowerResetListItemKey": "Reset",
            "PowerResetListItemValue": "|  \uE0F4",
            "PowerOffListItemKey": "Power off",
            "PowerOffListItemValue": "|  \uE098",
            "PowerOffListItemErrorText": "spsmShutdown failed!Error code: ",
            "WifiSwitchCategoryHeaderText": "WIFI toggle  |  \uE0E0 Toggle",
            "WifiSwitchCustomDrawerText": "\uE016  Quick toggle Switch WIFI status.",
            "WifiSwitchListItemKey": "WIFI status",
            "WifiSwitchStatusCheckErrorListItemText": "Check WIFI status failed! Error code: ",
            "WifiSwitchSetErrorListItemext": "nifmSetWirelessCommunicationEnabled failed! Error code: ",
            "SysmodulesNotFoundScanOKCustomDrawerText": "No sysmodules found!",
            "SysmodulesNotFoundScanNOKCustomDrawerText": "Scan failed!",
            "SysmodulesDynamicCategoryHeaderText": "Dynamic  |  \uE0E0  Toggle  |  \uE0E3  Toggle auto start",
            "SysmodulesDynamicCustomDrawerText": "\uE016  These sysmodules can be toggled at any time.",
            "SysmodulesStaticCategoryHeaderText": "Static  |  \uE0E3  Toggle auto start",
            "SysmodulesStaticCustomDrawerText": "\uE016  These sysmodules need a reboot to work.",
            "BootFileSwitchCategoryHeaderText": "Support CFW boot file switch  |  \uE0E0 Toggle",
            "BootFileSwitchCustomDrawerText": "\uE016  Takes effect after console restart.",
            "BootFileSXOSBootCopyNOKListItemText": "Select SXOS boot.dat failed! Error code: ",
            "BootFileSXGEARBootCopyNOKListItemText": "Select SXGEAR boot.dat failed! Error code: ",
            "BootFileSXOSBootSourceFileNotFoundListItemText": "Select SXOS boot.dat failed! Boot file not exist!",
            "BootFileSXGEARBootSourceFileNotFoundListItemText": "Select SXGEAR boot.dat failed! Boot file not exist!",
            "HekateRestartHitCategoryHeaderText": "Update hekate setting  |  \uE0E0 Toggle",
            "HekateRestartHitCustomDrawerText": "\uE016  Takes effect after toggle and console restart.",
            "HekateAutoRestartHitListItemKey": "Hekate restart without hit",
            "HekateAutoRestartHitINIParseNOKListItemValue": "INI parse failed",
            "HekateAutoRestartHitININoSectionListItemValue": "INI no such section",
            "HekateAutoRestartHitININoParameterListItemValue": "INI no such parameter",
            "HekateAutoRestartHitINIWriteNOKListItemValue": "INI write failed",
            "VersionSwitchCategoryHeaderText": "Mainland and international version switching  |  \uE0E2 Mainland  \uE0E3 international",
            "VersionSwitchCustomDrawerText": "\uE016  No need to wash white and initialize after switch. Just need restart and agree to the EULA",
            "VersionSwitchListItemKey": "Current version",
            "VersionSwitchMainlandListItemValue": "Mainland",
            "VersionSwitchInternationalListItemValue": "International",
            "VersionSwitchSetTNOKListItemValue": "setsysSetT failed, Error code: ",
            "VersionSwitchSetRegionCodeNOKListItemValue": "setsysSetRegionCode failed, Error code: ",
            "WlanCountryCodeSwitchCategoryHeaderText": "Mainland and Japen WLan country code switching  |  \uE0E2 Mainland  \uE0E3 Japan",
            "WlanCountryCodeSwitchCustomDrawerText": "\uE016  Suggest to backup the PRODINFO. Takes effect after restarted.",
            "WlanCountryCodeSwitchListItemKey": "Current WLan Country Code",
            "WlanCountryCodeSwitchMainlandListItemValue": "Mainland",
            "WlanCountryCodeSwitchJapanListItemValue": "Japan",
            "WlanCountryCodeSwitchSetTNOKListItemValue": "setWLANCountryCode failed! Error Code: "
        }
    )";
    std::string lanPath = std::string("sdmc:/switch/.overlays/lang/") + APPTITLE + "/";

    // Open a service manager session.
    if (R_FAILED(rc = smInitialize())) return;

    fsdevMountSdmc();
    tsl::hlp::doWithSmSession([&lanPath]{
        tsl::tr::InitTrans(lanPath, jsonStr);
    });

    std::string option;
    if (R_SUCCEEDED(rc = setGetIniConfig("sdmc:/config/" APPTITLE "/config.ini", APPTITLE, "powerControlEnabled", option)))
        this->m_sysmodEnabledFlags.powerControlEnabled = std::stoi(option);
    if (R_SUCCEEDED(rc = setGetIniConfig("sdmc:/config/" APPTITLE "/config.ini", APPTITLE, "wifiControlEnabled", option)))
        this->m_sysmodEnabledFlags.wifiControlEnabled = std::stoi(option);
    if (R_SUCCEEDED(rc = setGetIniConfig("sdmc:/config/" APPTITLE "/config.ini", APPTITLE, "sysmodulesControlEnabled", option)))
        this->m_sysmodEnabledFlags.sysmodulesControlEnabled = std::stoi(option);
    if (R_SUCCEEDED(rc = setGetIniConfig("sdmc:/config/" APPTITLE "/config.ini", APPTITLE, "bootFileControlEnabled", option)))
        this->m_sysmodEnabledFlags.bootFileControlEnabled = std::stoi(option);
    if (R_SUCCEEDED(rc = setGetIniConfig("sdmc:/config/" APPTITLE "/config.ini", APPTITLE, "hekateRestartControlEnabled", option)))
        this->m_sysmodEnabledFlags.hekateRestartControlEnabled = std::stoi(option);
    if (R_SUCCEEDED(rc = setGetIniConfig("sdmc:/config/" APPTITLE "/config.ini", APPTITLE, "consoleRegionControlEnabled", option)))
        this->m_sysmodEnabledFlags.consoleRegionControlEnabled = std::stoi(option);
#if 0
    if (R_SUCCEEDED(rc = setGetIniConfig("sdmc:/config/" APPTITLE "/config.ini", APPTITLE, "wlanCountryCodeControlEnabled", option)))
        this->m_sysmodEnabledFlags.wlanCountryCodeControlEnabled = std::stoi(option);
#endif

    /* Attempt to get the exosphere version. */
    if (R_SUCCEEDED(rc = splInitialize())) {
        u64 version{0};
        u32 version_micro{0xff};
        u32 version_minor{0xff};
        u32 version_major{0xff};
        if (R_SUCCEEDED(rc = splGetConfig(static_cast<SplConfigItem>(AMSVersionConfigItem), &version))) {
            version_micro = (version >> 40) & 0xff;
            version_minor = (version >> 48) & 0xff;
            version_major = (version >> 56) & 0xff;
        }
        splExit();
        if (version_major == 0 && version_minor == 0 && version_micro == 0) {
            if (this->m_sysmodEnabledFlags.bootFileControlEnabled)
                this->m_bootRunning = BootDatType::SXOS_BOOT_TYPE;
            std::strcpy(pathBuffer, sxosTitlesPath);
        } else if ((version_major == 0 && version_minor >= 9 && version_micro >= 0) || (version_major == 1 && version_minor >= 0 && version_micro >= 0)) {
            if (this->m_sysmodEnabledFlags.bootFileControlEnabled)
                this->m_bootRunning = BootDatType::SXGEAR_BOOT_TYPE;
            std::strcpy(pathBuffer, amsContentsPath);
        } else {
            return;
        }
    }

    if (this->m_sysmodEnabledFlags.consoleRegionControlEnabled) {
        this->m_isTencentVersion = false;
        if (R_FAILED(rc = setsysInitialize())) return;
        if (R_FAILED(rc = setsysGetT(&this->m_isTencentVersion))) {
            setsysExit();
            return;
        }
    }

#if 0
    if (R_FAILED(rc = getWLANCountryCode(this->m_curCountryCode))) return;
#endif

    if (this->m_sysmodEnabledFlags.sysmodulesControlEnabled) {
        FsDir contentDir;
        FsFileSystem fs;

        if (R_FAILED(fsOpenSdCardFileSystem(&fs))) {
            return;
        }
        tsl::hlp::ScopeGuard fsGuard1([&] { fsFsClose(&fs); });
    
        if (R_FAILED(rc = fsFsOpenDirectory(&fs, pathBuffer, FsDirOpenMode_ReadDirs, &contentDir))) {
            return;
        }

        tsl::hlp::ScopeGuard dirGuard([&] { fsDirClose(&contentDir); });

        boot2FlagFormat = std::string(pathBuffer) + boot2FlagFile;
        boot2FlagFolder = std::string(pathBuffer) + boot2FlagsFolder;
        std::string toolboxJsonFormat = std::string(pathBuffer) + toolboxJsonPath;

        /* Iterate over contents folder. */
        for (const auto &entry : FsDirIterator(contentDir)) {
            FsFile toolboxFile;
            std::snprintf(pathBuffer, FS_MAX_PATH, toolboxJsonFormat.c_str(), FS_MAX_PATH - 35, entry.name);
            if (R_FAILED(rc = fsFsOpenFile(&fs, pathBuffer, FsOpenMode_Read, &toolboxFile)))
                continue;
            tsl::hlp::ScopeGuard fileGuard([&] { fsFileClose(&toolboxFile); });

            /* Get toolbox file size. */
            s64 size;
            if (R_FAILED(rc = fsFileGetSize(&toolboxFile, &size)))
                continue;

            /* Read toolbox file. */
            std::string toolBoxData(size, '\0');
            u64 bytesRead;
            rc = fsFileRead(&toolboxFile, 0, toolBoxData.data(), size, FsReadOption_None, &bytesRead);
            if (R_FAILED(rc))
                continue;

            /* Parse toolbox file data. */
            json toolboxFileContent = json::parse(toolBoxData);

            u64 sysmoduleProgramId = std::strtoul(entry.name, nullptr, 16);

            /* Let's not allow Tesla to be killed with this. */
            if (sysmoduleProgramId == 0x420000000007E51AULL)
                continue;

            SystemModule module = {
                .listItem = new tsl::elm::ListItem(toolboxFileContent["name"]),
                .programId = sysmoduleProgramId,
                .needReboot = toolboxFileContent["requires_reboot"],
            };

            module.listItem->setClickListener([this, module](u64 click) -> bool {
                FsFileSystem fs;
                fsdevMountSdmc();
                if (R_FAILED(fsOpenSdCardFileSystem(&fs))) {
                    fsdevUnmountDevice("sdmc");
                    return false;
                }
                tsl::hlp::ScopeGuard fsGuard1([&] { fsFsClose(&fs); });

                /* if the folder "flags" does not exist, it will be created */
                std::snprintf(pathBuffer, FS_MAX_PATH, boot2FlagFolder.c_str(), module.programId);
                fsFsCreateDirectory(&fs, pathBuffer);
                std::snprintf(pathBuffer, FS_MAX_PATH, boot2FlagFormat.c_str(), module.programId);

                if (click & HidNpadButton_A && !module.needReboot) {
                    if (this->isRunning(module)) {
                        /* Kill process. */
                        pmshellTerminateProgram(module.programId);

                        /* Remove boot2 flag file. */
                        if (this->hasFlag(module))
                            fsFsDeleteFile(&fs, pathBuffer);
                    } else {
                        /* Start process. */
                        const NcmProgramLocation programLocation{
                            .program_id = module.programId,
                            .storageID = NcmStorageId_None,
                        };
                        u64 pid = 0;
                        pmshellLaunchProgram(0, &programLocation, &pid);

                        /* Create boot2 flag file. */
                        if (!this->hasFlag(module))
                            fsFsCreateFile(&fs, pathBuffer, 0, FsCreateOption(0));
                    }
                    fsdevUnmountDevice("sdmc");
                    return true;
                }

                if (click & HidNpadButton_Y) {
                    if (this->hasFlag(module)) {
                        /* Remove boot2 flag file. */
                        fsFsDeleteFile(&fs, pathBuffer);
                    } else {
                        /* Create boot2 flag file. */
                        fsFsCreateFile(&fs, pathBuffer, 0, FsCreateOption(0));
                    }
                    fsdevUnmountDevice("sdmc");
                    return true;
                }
                fsdevUnmountDevice("sdmc");
                return false;
            });
            this->m_sysmoduleListItems.push_back(std::move(module));
        }
        this->m_scanned = true;
    }
    fsdevUnmountDevice("sdmc");
}

GuiMain::~GuiMain() {
    smExit();
}

tsl::elm::Element *GuiMain::createUI() {
    tsl::elm::OverlayFrame *rootFrame = new tsl::elm::OverlayFrame("PluginName"_tr, VERSION);

    tsl::elm::List *sysmoduleList = new tsl::elm::List();

    if (this->m_sysmodEnabledFlags.powerControlEnabled) {
        sysmoduleList->addItem(new tsl::elm::CategoryHeader("PowerCategoryHeaderText"_tr, true));
        sysmoduleList->addItem(new tsl::elm::CustomDrawer([](tsl::gfx::Renderer *renderer, s32 x, s32 y, s32 w, s32 h) {
            renderer->drawString("PowerCustomDrawerText"_tr.c_str(), false, x + 5, y + 20, 15, renderer->a(tsl::style::color::ColorDescription));
        }), 30);
        tsl::elm::ListItem *powerResetListItem = new tsl::elm::ListItem("PowerResetListItemKey"_tr);
        powerResetListItem->setValue("PowerResetListItemValue"_tr);
        powerResetListItem->setClickListener([this, powerResetListItem](u64 click) -> bool {
            if (click & HidNpadButton_A) {
                Result rc;
                //if (R_FAILED(rc = bpcInitialize()) || R_FAILED(rc = bpcRebootSystem()))
                if (R_FAILED(rc = spsmInitialize()) || R_FAILED(rc = spsmShutdown(true)))
                    powerResetListItem->setText("PowerListItemErrorText"_tr + std::to_string(rc));
                spsmExit();
                //bpcExit();
                return true;
            }
            return false;
        });
        sysmoduleList->addItem(powerResetListItem);
        tsl::elm::ListItem *powerOffListItem = new tsl::elm::ListItem("PowerOffListItemKey"_tr);
        powerOffListItem->setValue("PowerOffListItemValue"_tr);
        powerOffListItem->setClickListener([this, powerOffListItem](u64 click) -> bool {
            if (click & HidNpadButton_A) {
                Result rc;
                //if (R_FAILED(rc = bpcInitialize()) || R_FAILED(rc = bpcShutdownSystem()))
                if (R_FAILED(rc = spsmInitialize()) || R_FAILED(rc = spsmShutdown(false)))
                    powerOffListItem->setText("PowerListItemErrorText"_tr + std::to_string(rc));
                spsmExit();
                //bpcExit();
                return true;
            }
            return false;
        });
        sysmoduleList->addItem(powerOffListItem);
    }

    if (this->m_sysmodEnabledFlags.wifiControlEnabled) {
        tsl::elm::CategoryHeader *wifiSwitchCatHeader = new tsl::elm::CategoryHeader("WifiSwitchCategoryHeaderText"_tr, true);
        sysmoduleList->addItem(wifiSwitchCatHeader);
        sysmoduleList->addItem(new tsl::elm::CustomDrawer([](tsl::gfx::Renderer *renderer, s32 x, s32 y, s32 w, s32 h) {
            renderer->drawString("WifiSwitchCustomDrawerText"_tr.c_str(), false, x + 5, y + 20, 15, renderer->a(tsl::style::color::ColorDescription));
        }), 30);
        this->m_listItemWifiSwitch = new tsl::elm::ListItem("WifiSwitchListItemKey"_tr);
        Result rc;
        if (R_SUCCEEDED(rc = nifmInitialize(NifmServiceType_Admin))) {
            if (R_FAILED(rc = nifmIsWirelessCommunicationEnabled(&this->m_isWifiOn))) {
                wifiSwitchCatHeader->setText("WifiSwitchStatusCheckErrorListItemText"_tr + std::to_string(rc));
            }
            nifmExit();
        }
        this->m_listItemWifiSwitch->setClickListener([this, wifiSwitchCatHeader](u64 click) -> bool {
            if (click == HidNpadButton_A) {
                Result rc;
                if (R_SUCCEEDED(rc = nifmInitialize(NifmServiceType_Admin))) {
                    if (R_FAILED(rc = nifmIsWirelessCommunicationEnabled(&this->m_isWifiOn))) {
                        wifiSwitchCatHeader->setText("WifiSwitchStatusCheckErrorListItemText"_tr + std::to_string(rc));
                    } else {
                        if (R_FAILED(rc = nifmSetWirelessCommunicationEnabled(this->m_isWifiOn = !this->m_isWifiOn))) {
                            wifiSwitchCatHeader->setText("WifiSwitchSetErrorListItemext"_tr + std::to_string(rc));
                        }
                        svcSleepThread(500*1000*1000);
                    }
                    nifmExit();
                }
                if (R_FAILED(rc))
                    return false;
                else
                    return true;
            }
            return false;
        });
        sysmoduleList->addItem(this->m_listItemWifiSwitch);
    }

    if (this->m_sysmodEnabledFlags.sysmodulesControlEnabled) {
        if (this->m_sysmoduleListItems.size() == 0) {
            std::string description = this->m_scanned ? "SysmodulesNotFoundScanOKCustomDrawerText"_tr : "SysmodulesNotFoundScanNOKCustomDrawerText"_tr;
            auto *warning = new tsl::elm::CustomDrawer([description](tsl::gfx::Renderer *renderer, s32 x, s32 y, s32 w, s32 h) {
                renderer->drawString("\uE150", false, 180, 250, 90, renderer->a(0xFFFF));
                renderer->drawString(description.c_str(), false, 110, 340, 25, renderer->a(0xFFFF));
            });
            sysmoduleList->addItem(warning);
        } else {
            sysmoduleList->addItem(new tsl::elm::CategoryHeader("SysmodulesDynamicCategoryHeaderText"_tr, true));
            sysmoduleList->addItem(new tsl::elm::CustomDrawer([](tsl::gfx::Renderer *renderer, s32 x, s32 y, s32 w, s32 h) {
                renderer->drawString("SysmodulesDynamicCustomDrawerText"_tr.c_str(), false, x + 5, y + 20, 15, renderer->a(tsl::style::color::ColorDescription));
            }), 30);
            for (const auto &module : this->m_sysmoduleListItems) {
                if (!module.needReboot)
                    sysmoduleList->addItem(module.listItem);
            }
            sysmoduleList->addItem(new tsl::elm::CategoryHeader("SysmodulesStaticCategoryHeaderText"_tr, true));
            sysmoduleList->addItem(new tsl::elm::CustomDrawer([](tsl::gfx::Renderer *renderer, s32 x, s32 y, s32 w, s32 h) {
                renderer->drawString("SysmodulesStaticCustomDrawerText"_tr.c_str(), false, x + 5, y + 20, 15, renderer->a(tsl::style::color::ColorDescription));
            }), 30);
            for (const auto &module : this->m_sysmoduleListItems) {
                if (module.needReboot)
                    sysmoduleList->addItem(module.listItem);
            }
        }
    }

    if (this->m_sysmodEnabledFlags.bootFileControlEnabled) {
        tsl::elm::CategoryHeader *bootCatHeader = new tsl::elm::CategoryHeader("BootFileSwitchCategoryHeaderText"_tr, true);
        sysmoduleList->addItem(bootCatHeader);
        sysmoduleList->addItem(new tsl::elm::CustomDrawer([](tsl::gfx::Renderer *renderer, s32 x, s32 y, s32 w, s32 h) {
            renderer->drawString("BootFileSwitchCustomDrawerText"_tr.c_str(), false, x + 5, y + 20, 15, renderer->a(tsl::style::color::ColorDescription));
        }), 30);
        this->m_listItemSXOSBootType = new tsl::elm::ListItem(bootFiledescriptions[0]);
        this->m_listItemSXOSBootType->setClickListener([this, bootCatHeader](u64 click) -> bool {
            if (click & HidNpadButton_A) {
                if (this->m_bootRunning == BootDatType::SXOS_BOOT_TYPE) return true;
                Result rc;
                rc = this->CopyFile("/bootloader/boot-sxos.dat", "/boot.dat");
                if (R_FAILED(rc)) {
                    if (rc == 514) {
                        bootCatHeader->setText("BootFileSXOSBootSourceFileNotFoundListItemText"_tr);
                    } else {
                        bootCatHeader->setText("BootFileSXOSBootCopyNOKListItemText"_tr + std::to_string(rc));
                    }
                    return false;
                }
                this->m_bootRunning = BootDatType::SXOS_BOOT_TYPE;
                return true;
            }
            return false;
        });
        sysmoduleList->addItem(this->m_listItemSXOSBootType);
        this->m_listItemSXGEARBootType = new tsl::elm::ListItem(bootFiledescriptions[1]);
        this->m_listItemSXGEARBootType->setClickListener([this, bootCatHeader](u64 click) -> bool {
            if (click & HidNpadButton_A) {
                if (this->m_bootRunning == BootDatType::SXGEAR_BOOT_TYPE) return true;
                Result rc;
                rc = CopyFile("/bootloader/boot-sxgear.dat", "/boot.dat");
                if (R_FAILED(rc)) {
                    if (rc == 514) {
                        bootCatHeader->setText("BootFileSXGEARBootSourceFileNotFoundListItemText"_tr);
                    } else {
                        bootCatHeader->setText("BootFileSXGEARBootCopyNOKListItemText"_tr + std::to_string(rc));
                    }
                    return false;
                }
                this->m_bootRunning = BootDatType::SXGEAR_BOOT_TYPE;
                return true;
            }
            return false;
        });
        sysmoduleList->addItem(this->m_listItemSXGEARBootType);
    }

    if (this->m_sysmodEnabledFlags.hekateRestartControlEnabled) {
        sysmoduleList->addItem(new tsl::elm::CategoryHeader("HekateRestartHitCategoryHeaderText"_tr, true));
        sysmoduleList->addItem(new tsl::elm::CustomDrawer([](tsl::gfx::Renderer *renderer, s32 x, s32 y, s32 w, s32 h) {
            renderer->drawString("HekateRestartHitCustomDrawerText"_tr.c_str(), false, x + 5, y + 20, 15, renderer->a(tsl::style::color::ColorDescription));
        }), 30);
        tsl::elm::ListItem *opAutoboot = new tsl::elm::ListItem("HekateAutoRestartHitListItemKey"_tr);
        static std::string autobootValue = "-1";
        Result rc = setGetIniConfig("/bootloader/hekate_ipl.ini", "config", "autoboot", autobootValue);
        switch (rc)
        {
        case 1:
            opAutoboot->setValue("HekateAutoRestartHitINIParseNOKListItemValue"_tr);
            break;
        case 2:
            opAutoboot->setValue("HekateAutoRestartHitININoSectionListItemValue"_tr);
            break;
        case 3:
            opAutoboot->setValue("HekateAutoRestartHitININoParameterListItemValue"_tr);
            break;
        default:
            break;
        }
        if (rc) {
            sysmoduleList->addItem(opAutoboot);
            rootFrame->setContent(sysmoduleList);
            return rootFrame;
        }
        opAutoboot->setValue(autobootValue);
        opAutoboot->setClickListener([this, opAutoboot](u64 click) -> bool {
            if (click & HidNpadButton_A) {
                if (autobootValue == "1")
                    autobootValue = "0";
                else if (autobootValue == "0")
                    autobootValue = "1";
                else
                    autobootValue = "-1";
                opAutoboot->setValue(autobootValue);
                Result rc;
                rc = setGetIniConfig("/bootloader/hekate_ipl.ini", "config", "autoboot", autobootValue, false);
                switch (rc)
                {
                case 1:
                    opAutoboot->setValue("HekateAutoRestartHitINIParseNOKListItemValue"_tr);
                    break;
                case 2:
                    opAutoboot->setValue("HekateAutoRestartHitININoSectionListItemValue"_tr);
                    break;
                case 3:
                    opAutoboot->setValue("HekateAutoRestartHitININoParameterListItemValue"_tr);
                    break;
                case 4:
                    opAutoboot->setValue("HekateAutoRestartHitINIWriteNOKListItemValue"_tr);
                    break;
                default:
                    break;
                }

                if (rc) return false;

                return true;
            }
            return false;
        });
        sysmoduleList->addItem(opAutoboot);
    }

    if (this->m_sysmodEnabledFlags.consoleRegionControlEnabled) {
        sysmoduleList->addItem(new tsl::elm::CategoryHeader("VersionSwitchCategoryHeaderText"_tr, true));
        sysmoduleList->addItem(new tsl::elm::CustomDrawer([](tsl::gfx::Renderer *renderer, s32 x, s32 y, s32 w, s32 h) {
            renderer->drawString("VersionSwitchCustomDrawerText"_tr.c_str(), false, x + 5, y + 20, 15, renderer->a(tsl::style::color::ColorDescription));
        }), 30);
        tsl::elm::ListItem *verSwitchItem = new tsl::elm::ListItem("VersionSwitchListItemKey"_tr);
        verSwitchItem->setValue(this->m_isTencentVersion ? "VersionSwitchMainlandListItemValue"_tr : "VersionSwitchInternationalListItemValue"_tr);
        verSwitchItem->setClickListener([this, verSwitchItem](u64 click) -> bool {
            Result rc;
            if (click & HidNpadButton_X) {
                if (this->m_isTencentVersion) return true;
                if (R_FAILED(rc = setsysInitialize())) return false;
                if (R_FAILED(rc = setsysSetT(true))) {
                    setsysExit();
                    verSwitchItem->setText("VersionSwitchSetTNOKListItemValue"_tr + std::to_string(rc));
                    return false;
                }
                if (R_FAILED(rc = setsysSetRegionCode(SetRegion_CHN))) {
                    setsysExit();
                    verSwitchItem->setText("VersionSwitchSetRegionCodeNOKListItemValue"_tr + std::to_string(rc));
                    return false;
                }
                setsysExit();
                this->m_isTencentVersion = true;
                verSwitchItem->setValue("VersionSwitchMainlandListItemValue"_tr);
                return true;
            } else if (click & HidNpadButton_Y) {
                if (!this->m_isTencentVersion) return true;
                if (R_FAILED(rc = setsysInitialize())) return false;
                if (R_FAILED(rc = setsysSetT(false))) {
                    setsysExit();
                    verSwitchItem->setText("VersionSwitchSetTNOKListItemValue"_tr + std::to_string(rc));
                    return false;
                }
                if (R_FAILED(rc = setsysSetRegionCode(SetRegion_HTK))) {
                    setsysExit();
                    verSwitchItem->setText("VersionSwitchSetRegionCodeNOKListItemValue"_tr + std::to_string(rc));
                    return false;
                }
                setsysExit();
                this->m_isTencentVersion = false;
                verSwitchItem->setValue("VersionSwitchInternationalListItemValue"_tr);
                return true;
            }
            return false;
        });
        sysmoduleList->addItem(verSwitchItem);
    }

#if 0
    if (this->m_sysmodEnabledFlags.wlanCountryCodeControlEnabled) {
        sysmoduleList->addItem(new tsl::elm::CategoryHeader("WlanCountryCodeSwitchCategoryHeaderText"_tr, true));
        sysmoduleList->addItem(new tsl::elm::CustomDrawer([](tsl::gfx::Renderer *renderer, s32 x, s32 y, s32 w, s32 h) {
            renderer->drawString("WlanCountryCodeSwitchCustomDrawerText"_tr.c_str(), false, x + 5, y + 20, 15, renderer->a(tsl::style::color::ColorDescription));
        }), 30);
        tsl::elm::ListItem *wlanCountryCodeSwitchItem = new tsl::elm::ListItem("WlanCountryCodeSwitchListItemKey"_tr);
        wlanCountryCodeSwitchItem->setValue((this->m_curCountryCode.compare(CHINA_WLAN_COUNTRY_CODE) == 0) ? "WlanCountryCodeSwitchMainlandListItemValue"_tr : "WlanCountryCodeSwitchJapanListItemValue"_tr);
        wlanCountryCodeSwitchItem->setClickListener([this, wlanCountryCodeSwitchItem](u64 click) -> bool {
            Result rc;
            if (click & HidNpadButton_X) {
                if (this->m_curCountryCode.compare(CHINA_WLAN_COUNTRY_CODE) == 0) return true;
                if (R_FAILED(rc = this->setWLANCountryCode(CHINA_WLAN_COUNTRY_CODE))) {
                    wlanCountryCodeSwitchItem->setText("WlanCountryCodeSwitchSetTNOKListItemValue"_tr + std::to_string(rc));
                    return false;
                }
                this->m_curCountryCode = CHINA_WLAN_COUNTRY_CODE;
                wlanCountryCodeSwitchItem->setValue("WlanCountryCodeSwitchMainlandListItemValue"_tr);
                return true;
            } else if (click & HidNpadButton_Y) {
                if (this->m_curCountryCode.compare(JAPAN_WLAN_COUNTRY_CODE) == 0) return true;
                if (R_FAILED(rc = this->setWLANCountryCode(JAPAN_WLAN_COUNTRY_CODE))) {
                    wlanCountryCodeSwitchItem->setText("WlanCountryCodeSwitchSetTNOKListItemValue"_tr + std::to_string(rc));
                    return false;
                }
                this->m_curCountryCode = JAPAN_WLAN_COUNTRY_CODE;
                wlanCountryCodeSwitchItem->setValue("WlanCountryCodeSwitchJapanListItemValue"_tr);
                return true;
            }
            return false;
        });
        sysmoduleList->addItem(wlanCountryCodeSwitchItem);
    }
#endif

    rootFrame->setContent(sysmoduleList);

    return rootFrame;
}

Result GuiMain::setGetIniConfig(std::string iniPath, std::string iniSection, std::string iniOption, std::string &iniValue, bool getOption) {
    Result ret = 0;
    FsFileSystem fs;

    if (R_FAILED(ret = fsOpenSdCardFileSystem(&fs))) {
        return ret;
    }

    tsl::hlp::ScopeGuard fsGuard1([&] { fsFsClose(&fs); });

    simpleIniParser::Ini *ini = simpleIniParser::Ini::parseFile(iniPath);
    if (!ini) ret = 1;
    simpleIniParser::IniSection *section = ini->findSection(iniSection);
    if (!section) ret = 2;
    simpleIniParser::IniOption *option = section->findFirstOption(iniOption);
    if (!option) ret = 3;

    if (!ret) {
        if (getOption) {
            iniValue = option->value;
        } else {
            option->value = iniValue;
            if (!(ini->writeToFile(iniPath))) ret = 4;
        }
    }

    return ret;
}

Result GuiMain::CopyFile(const char *srcPath, const char *destPath) {
    Result ret{0};
    FsFileSystem fs;

    if (R_FAILED(ret = fsOpenSdCardFileSystem(&fs))) {
        return ret;
    }

    tsl::hlp::ScopeGuard fsGuard1([&] { fsFsClose(&fs); });

    FsFile src_handle, dest_handle;
    if (R_FAILED(ret = fsFsOpenFile(&fs, srcPath, FsOpenMode_Read, &src_handle))) {
        return ret;
    }
    tsl::hlp::ScopeGuard fileGuard1([&] { fsFileClose(&src_handle); });

    s64 size = 0;
    if (R_FAILED(ret = fsFileGetSize(&src_handle, &size))) {
        return ret;
    }

    if (R_SUCCEEDED(fsFsOpenFile(&fs, destPath, FsOpenMode_Read, &dest_handle))) {
        fsFileClose(&dest_handle);
        if (R_FAILED(ret = fsFsDeleteFile(&fs, destPath))) {
            return ret;
        }
	    if (R_FAILED(ret = fsFsCreateFile(&fs, destPath, size, 0))) {
            return ret;
        }
    }

    if (R_FAILED(ret = fsFsOpenFile(&fs, destPath, FsOpenMode_Write, &dest_handle))) {
        return ret;
    }
    tsl::hlp::ScopeGuard fileGuard2([&] { fsFileClose(&dest_handle); });

    u64 bytes_read = 0;
    const u64 buf_size = 0x10000;
    s64 offset = 0;
    unsigned char *buf = new unsigned char[buf_size];
    tsl::hlp::ScopeGuard fileGuard3([&] { delete[] buf; });
    std::string filename = std::filesystem::path(srcPath).filename();

    do {
        std::memset(buf, 0, buf_size);
        if (R_FAILED(ret = fsFileRead(&src_handle, offset, buf, buf_size, FsReadOption_None, &bytes_read))) {
            return ret;
        }
        if (R_FAILED(ret = fsFileWrite(&dest_handle, offset, buf, bytes_read, FsWriteOption_Flush))) {
            return ret;
        }
        offset += bytes_read;
    } while(offset < size);

    return ret;
}

void GuiMain::update() {
    static u8 counter = 0;

    if (counter++ % 20 != 0)
        return;
    else
        counter = 0;

    if (this->m_sysmodEnabledFlags.sysmodulesControlEnabled) {
        for (const auto &module : this->m_sysmoduleListItems) {
            this->updateStatus(module);
        }
    }

    if (this->m_sysmodEnabledFlags.bootFileControlEnabled) {
        this->m_listItemSXOSBootType->setValue((this->m_bootRunning == BootDatType::SXOS_BOOT_TYPE) ? "RunAndHasFlagListItemValue"_tr : "NotRunAndNoFlagListItemValue"_tr);
        this->m_listItemSXGEARBootType->setValue((this->m_bootRunning == BootDatType::SXGEAR_BOOT_TYPE) ? "RunAndHasFlagListItemValue"_tr : "NotRunAndNoFlagListItemValue"_tr);
    }

    if (this->m_sysmodEnabledFlags.wifiControlEnabled) {
        this->m_listItemWifiSwitch->setValue(this->m_isWifiOn ? "RunAndHasFlagListItemValue"_tr : "NotRunAndNoFlagListItemValue"_tr);
    }
}

void GuiMain::updateStatus(const SystemModule &module) {
    bool running = this->isRunning(module);
    bool hasFlag = this->hasFlag(module);

    static const std::string descriptions[2][2] = {
        [0] = {
            [0] = "NotRunAndNoFlagListItemValue"_tr,
            [1] = "NotRunAndHasFlagListItemValue"_tr,
        },
        [1] = {
            [0] = "RunAndNoFlagListItemValue"_tr,
            [1] = "RunAndHasFlagListItemValue"_tr,
        },
    };

    module.listItem->setValue(descriptions[running][hasFlag]);
}

bool GuiMain::hasFlag(const SystemModule &module) {
    FsFileSystem fs;

    if (R_FAILED(fsOpenSdCardFileSystem(&fs))) {
        return false;
    }

    tsl::hlp::ScopeGuard fsGuard1([&] { fsFsClose(&fs); });

    FsFile flagFile;
    std::snprintf(pathBuffer, FS_MAX_PATH, boot2FlagFormat.c_str(), module.programId);
    Result rc = fsFsOpenFile(&fs, pathBuffer, FsOpenMode_Read, &flagFile);
    if (R_SUCCEEDED(rc)) {
        fsFileClose(&flagFile);
        return true;
    } else {
        return false;
    }
}

bool GuiMain::isRunning(const SystemModule &module) {
    u64 pid = 0;
    if (R_FAILED(pmdmntGetProcessId(&pid, module.programId)))
        return false;
    return pid > 0;
}

#if 0
Result GuiMain::setWLANCountryCode(std::string wlanCountCode)
{
	Result rc = 1;
	FsStorage fs;
	u32 calibrationDataSize = 0;
	u8 oriHash[BodyHash_SIZE];
	u8 newHash[BodyHash_SIZE];
	u8 oriWlanCountCode[WlanCountryCode_SIZE] = {};
	u8* buffer = nullptr;

	if (R_SUCCEEDED(rc = fsOpenBisStorage(&fs, FsBisPartitionId_CalibrationBinary))) {
		if (R_SUCCEEDED(rc = fsStorageRead(&fs, BodySize_OFFSET, &calibrationDataSize, sizeof(calibrationDataSize)))) {
			if (R_SUCCEEDED(rc = fsStorageRead(&fs, BodyHash_OFFSET, oriHash, sizeof(oriHash)))) {
                if (R_SUCCEEDED(rc = fsStorageRead(&fs, WlanCountryCodes_OFFSET, oriWlanCountCode, WlanCountryCode_SIZE))) {
                    // 写之前要加密？
                    if (R_SUCCEEDED(rc = fsStorageWrite(&fs, WlanCountryCodes_OFFSET, wlanCountCode.c_str(), WlanCountryCode_SIZE))) {
                        buffer = new u8[calibrationDataSize];
		                if (buffer) {
                            if (R_SUCCEEDED(rc = fsStorageRead(&fs, BodyHashBeigin_OFFSET, buffer, calibrationDataSize))) {
                                sha256CalculateHash(newHash, buffer, calibrationDataSize);
                                if (R_FAILED(rc = fsStorageWrite(&fs, BodyHash_OFFSET, newHash, BodyHash_SIZE))) {
                                    if (R_SUCCEEDED(rc = fsStorageRead(&fs, BodyHashBeigin_OFFSET, buffer, calibrationDataSize))) {
                                        sha256CalculateHash(newHash, buffer, calibrationDataSize);
                                        if (R_FAILED(rc = memcmp(oriHash, newHash, sizeof(oriHash)))) {
                                            if (R_FAILED(rc = fsStorageWrite(&fs, WlanCountryCodes_OFFSET, oriWlanCountCode, WlanCountryCode_SIZE))) {
                                                rc = fsStorageWrite(&fs, BodyHash_OFFSET, oriHash, BodyHash_SIZE);
                                            }
                                        }
                                    }
                                }
                            }
                            delete buffer;
			                buffer = nullptr;
                        }
                    }
                }
			}
		}
        fsStorageClose(&fs);
	}

	return rc;
}

Result GuiMain::getWLANCountryCode(std::string &outWlanCountCode)
{
	Result rc = 1;
    SetCalCountryCode wlanCountrycodes{};
    s32 wlanCountryCodeTotalcounts = 0;
    if (R_SUCCEEDED(rc = setcalInitialize())) {
        if (R_SUCCEEDED(rc = setcalGetWirelessLanCountryCodes(&wlanCountryCodeTotalcounts, &wlanCountrycodes, 1))) {
            outWlanCountCode = wlanCountrycodes.code;
        } else {
            outWlanCountCode = "";
        }
        setcalExit();
    }
	return rc;
}
#endif