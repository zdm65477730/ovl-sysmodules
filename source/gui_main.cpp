#include "gui_main.hpp"

#include "dir_iterator.hpp"

#include <SimpleIniParser.hpp>
#include <json.hpp>
#include <filesystem>

using json = nlohmann::json;

constexpr const char *const amsContentsPath = "/atmosphere/contents";
//constexpr const char *const boot2FlagFormat = "/atmosphere/contents/%016lX/flags/boot2.flag";
constexpr const char *const sxosTitlesPath = "/sxos/titles";
constexpr const char *const boot2FlagPath = "/%016lX/flags/boot2.flag";
constexpr const char *const toolboxJsonPath = "/%s/toolbox.json";

static constexpr u32 AMSVersionConfigItem = 65000;
//static constexpr s64 SXOS_MIN_BOOT_SIZE = 10 * 1024 * 1024;

static std::string boot2FlagFormat{amsContentsPath};
static char pathBuffer[FS_MAX_PATH];

constexpr const char *const descriptions[2][2] = {
    [0] = {
        [0] = "Off | \uE098",
        [1] = "Off | \uE0F4",
    },
    [1] = {
        [0] = "On | \uE098",
        [1] = "On | \uE0F4",
    },
};

constexpr const char *const bootFiledescriptions[2] = {
        [0] = "SXOS boot.dat",
        [1] = "SXGEAR boot.dat"
};

constexpr const char *const bootFileSrcPath[3] = {
        [0] = "/bootloader/boot-sxos.dat",
        [1] = "/bootloader/boot-sxgear.dat",
        [2] = "/boot.dat"
};

GuiMain::GuiMain() {
    Result rc;
    // Open a service manager session.
    if (R_FAILED(rc = smInitialize())) return;

    /* Attempt to get the exosphere version. */
    if (R_FAILED(rc = splInitialize())) return;
    u64 version{0};
    u32 version_micro{0xff};
    u32 version_minor{0xff};
    u32 version_major{0xff};
    if (R_SUCCEEDED(rc = splGetConfig(static_cast<SplConfigItem>(AMSVersionConfigItem), &version))) {
        version_micro = (version >> 40) & 0xff;
        version_minor = (version >> 48) & 0xff;
        version_major = (version >> 56) & 0xff;
    }
    if (version_major == 0 && version_minor == 0 && version_micro == 0) {
        this->m_bootRunning = BootDatType::SXOS_BOOT_TYPE;
        std::strcpy(pathBuffer, sxosTitlesPath);
    } else if (version_major >= 0 && version_minor >= 7 && version_micro >= 0) {
        this->m_bootRunning = BootDatType::SXGEAR_BOOT_TYPE;
        std::strcpy(pathBuffer, amsContentsPath);
    } else
        return;
    splExit();

    this->m_isTencentVersion = false;
    if (R_FAILED(rc = setInitialize())) return;
    if (R_FAILED(rc = setsysInitialize())) return;
    // Get bool of IsT (isTencent).
    if (R_FAILED(rc = setsysGetT(&this->m_isTencentVersion))) return;

    rc = fsOpenSdCardFileSystem(&this->m_fs);
    if (R_FAILED(rc)) return;

#if 0
    this->m_bootSize = 0;
	FsFile bootHandle;
	if (R_FAILED(fsFsOpenFile(&this->m_fs, bootFileSrcPath[2], FsOpenMode_Read, &bootHandle))) return;
    tsl::hlp::ScopeGuard fileGuard([&] { fsFileClose(&bootHandle); });
    if (R_FAILED(fsFileGetSize(&bootHandle, &m_bootSize))) return;
    if (this->m_bootSize < SXOS_MIN_BOOT_SIZE) {
        this->m_bootRunning = BootDatType::SXGEAR_BOOT_TYPE;
        std::strcpy(pathBuffer, amsContentsPath);
    } else {
        this->m_bootRunning = BootDatType::SXOS_BOOT_TYPE;
        std::strcpy(pathBuffer, sxosTitlesPath);
    }
#endif

    FsDir contentDir;
    rc = fsFsOpenDirectory(&this->m_fs, pathBuffer, FsDirOpenMode_ReadDirs, &contentDir);
    if (R_FAILED(rc)) return;

    tsl::hlp::ScopeGuard dirGuard([&] { fsDirClose(&contentDir); });

    boot2FlagFormat = std::string(pathBuffer) + boot2FlagPath;
    std::string toolboxJsonFormat = std::string(pathBuffer) + toolboxJsonPath;

    /* Iterate over contents folder. */
    for (const auto &entry : FsDirIterator(contentDir)) {
        FsFile toolboxFile;
        std::snprintf(pathBuffer, FS_MAX_PATH, toolboxJsonFormat.c_str(), entry.name);
        rc = fsFsOpenFile(&this->m_fs, pathBuffer, FsOpenMode_Read, &toolboxFile);
        if (R_FAILED(rc))
            continue;
        tsl::hlp::ScopeGuard fileGuard([&] { fsFileClose(&toolboxFile); });

        /* Get toolbox file size. */
        s64 size;
        rc = fsFileGetSize(&toolboxFile, &size);
        if (R_FAILED(rc))
            continue;

        /* Read toolbox file. */
        std::string toolBoxData(size, '\0');
        u64 bytesRead;
        rc = fsFileRead(&toolboxFile, 0, toolBoxData.data(), size, FsReadOption_None, &bytesRead);
        if (R_FAILED(rc))
            continue;

        /* Parse toolbox file data. */
        json toolboxFileContent = json::parse(toolBoxData);

        const std::string &sysmoduleProgramIdString = toolboxFileContent["tid"];
        u64 sysmoduleProgramId = std::strtoul(sysmoduleProgramIdString.c_str(), nullptr, 16);

        /* Let's not allow Tesla to be killed with this. */
        if (sysmoduleProgramId == 0x420000000007E51AULL)
            continue;

        SystemModule module = {
            .listItem = new tsl::elm::ListItem(toolboxFileContent["name"]),
            .programId = sysmoduleProgramId,
            .needReboot = toolboxFileContent["requires_reboot"],
        };

        module.listItem->setClickListener([this, module](u64 click) -> bool {
            if (click & HidNpadButton_A && !module.needReboot) {
                if (this->isRunning(module)) {
                    /* Kill process. */
                    pmshellTerminateProgram(module.programId);
                } else {
                    /* Start process. */
                    const NcmProgramLocation programLocation{
                        .program_id = module.programId,
                        .storageID = NcmStorageId_None,
                    };
                    u64 pid = 0;
                    pmshellLaunchProgram(0, &programLocation, &pid);
                }
                return true;
            }

            if (click & HidNpadButton_Y) {
                std::snprintf(pathBuffer, FS_MAX_PATH, boot2FlagFormat.c_str(), module.programId);
                if (this->hasFlag(module)) {
                    /* Remove boot2 flag file. */
                    fsFsDeleteFile(&this->m_fs, pathBuffer);
                } else {
                    /* Create boot2 flag file. */
                    fsFsCreateFile(&this->m_fs, pathBuffer, 0, FsCreateOption(0));
                }
                return true;
            }

            return false;
        });
        this->m_sysmoduleListItems.push_back(std::move(module));
    }
    this->m_scanned = true;
}

GuiMain::~GuiMain() {
    fsFsClose(&this->m_fs);

    setsysExit();
    setExit();

    // Close the service manager session.
    smExit();
}

tsl::elm::Element *GuiMain::createUI() {
    tsl::elm::OverlayFrame *rootFrame = new tsl::elm::OverlayFrame("Sysmodules", VERSION);

    tsl::elm::List *sysmoduleList = new tsl::elm::List();

    sysmoduleList->addItem(new tsl::elm::CategoryHeader("SWITCH电源控制  |  \uE0E0  重启和关机", true));
    sysmoduleList->addItem(new tsl::elm::CustomDrawer([](tsl::gfx::Renderer *renderer, s32 x, s32 y, s32 w, s32 h) {
        renderer->drawString("\uE016  快速重启或者关闭您的SWITCH。", false, x + 5, y + 20, 15, renderer->a(tsl::style::color::ColorDescription));
    }), 30);

    tsl::elm::ListItem *powerResetListItem = new tsl::elm::ListItem("重启");
    powerResetListItem->setValue("|  \uE0F4");
    powerResetListItem->setClickListener([this, powerResetListItem](u64 click) -> bool {
        if (click & KEY_A) {
            Result rc;
            //if (R_FAILED(rc = bpcInitialize()) || R_FAILED(rc = bpcRebootSystem()))
            if (R_FAILED(rc = spsmInitialize()) || R_FAILED(rc = spsmShutdown(true)))
                powerResetListItem->setText(std::string("spsmShutdown失败！错误码：" + std::to_string(rc)));
            spsmExit();
            //bpcExit();
            return true;
        }
        return false;
    });
    sysmoduleList->addItem(powerResetListItem);

    tsl::elm::ListItem *powerOffListItem = new tsl::elm::ListItem("关机");
    powerOffListItem->setValue("|  \uE098");
    powerOffListItem->setClickListener([this, powerOffListItem](u64 click) -> bool {
        if (click & KEY_A) {
            Result rc;
            //if (R_FAILED(rc = bpcInitialize()) || R_FAILED(rc = bpcShutdownSystem()))
            if (R_FAILED(rc = spsmInitialize()) || R_FAILED(rc = spsmShutdown(false)))
                powerOffListItem->setText(std::string("spsmShutdown失败！错误码：" + std::to_string(rc)));
            spsmExit();
            //bpcExit();
            return true;
        }
        return false;
    });
    sysmoduleList->addItem(powerOffListItem);

    if (this->m_sysmoduleListItems.size() == 0) {
        const char *description = this->m_scanned ? "没有找到任何系统模块！" : "检索失败！";
        auto *warning = new tsl::elm::CustomDrawer([description](tsl::gfx::Renderer *renderer, s32 x, s32 y, s32 w, s32 h) {
            renderer->drawString("\uE150", false, 180, 250, 90, renderer->a(0xFFFF));
            renderer->drawString(description, false, 110, 340, 25, renderer->a(0xFFFF));
        });
        sysmoduleList->addItem(warning);
    } else {
        sysmoduleList->addItem(new tsl::elm::CategoryHeader("动态  |  \uE0E0 切换  |  \uE0E3 自动启动", true));
        sysmoduleList->addItem(new tsl::elm::CustomDrawer([](tsl::gfx::Renderer *renderer, s32 x, s32 y, s32 w, s32 h) {
            renderer->drawString("\uE016  这些系统模块可以任何时候切换。", false, x + 5, y + 20, 15, renderer->a(tsl::style::color::ColorDescription));
        }), 30);
        for (const auto &module : this->m_sysmoduleListItems) {
            if (!module.needReboot)
                sysmoduleList->addItem(module.listItem);
        }

        sysmoduleList->addItem(new tsl::elm::CategoryHeader("静态  |  \uE0E3 自动启动", true));
        sysmoduleList->addItem(new tsl::elm::CustomDrawer([](tsl::gfx::Renderer *renderer, s32 x, s32 y, s32 w, s32 h) {
            renderer->drawString("\uE016  这些系统模块需要重启切换。", false, x + 5, y + 20, 15, renderer->a(tsl::style::color::ColorDescription));
        }), 30);
        for (const auto &module : this->m_sysmoduleListItems) {
            if (module.needReboot)
                sysmoduleList->addItem(module.listItem);
        }
    }

    tsl::elm::CategoryHeader *bootCatHeader = new tsl::elm::CategoryHeader("支持系统引导的Boot文件  |  \uE0E0 切换", true);
    sysmoduleList->addItem(bootCatHeader);
    sysmoduleList->addItem(new tsl::elm::CustomDrawer([](tsl::gfx::Renderer *renderer, s32 x, s32 y, s32 w, s32 h) {
        renderer->drawString("\uE016  切换后重启生效。", false, x + 5, y + 20, 15, renderer->a(tsl::style::color::ColorDescription));
    }), 30);

    this->m_listItem1 = new tsl::elm::ListItem(bootFiledescriptions[0]);
    this->m_listItem1->setValue((this->m_bootRunning == BootDatType::SXOS_BOOT_TYPE) ? "正在使用 | \uE0F4" : "未启用 | \uE098");
    this->m_listItem1->setClickListener([this, bootCatHeader](u64 click) -> bool {
        if (click & KEY_A) {
            if (this->m_bootRunning == BootDatType::SXOS_BOOT_TYPE) return true;
            this->m_bootRunning = BootDatType::SXOS_BOOT_TYPE;
            Result rc = this->CopyFile(bootFileSrcPath[0], bootFileSrcPath[2]);
            if (R_FAILED(rc)) {
                bootCatHeader->setText(std::string("切换SXOS Boot.dat失败！错误码：") + std::to_string(rc));
                return false;
            }
            return true;
        }
        return false;
    });
    sysmoduleList->addItem(this->m_listItem1);

	this->m_listItem2 = new tsl::elm::ListItem(bootFiledescriptions[1]);
    this->m_listItem2->setValue((this->m_bootRunning == BootDatType::SXGEAR_BOOT_TYPE) ? "正在使用 | \uE0F4" : "未启用 | \uE098");
    this->m_listItem2->setClickListener([this, bootCatHeader](u64 click) -> bool {
        if (click & KEY_A) {
            if (this->m_bootRunning == BootDatType::SXGEAR_BOOT_TYPE) return true;
            this->m_bootRunning = BootDatType::SXGEAR_BOOT_TYPE;
            Result rc = CopyFile(bootFileSrcPath[1], bootFileSrcPath[2]);
            if (R_FAILED(rc)) {
                bootCatHeader->setText(std::string("切换SXGEAR Boot.dat失败！错误码：") + std::to_string(rc));
                return false;
            }
            return true;
        }
        return false;
    });
    sysmoduleList->addItem(this->m_listItem2);

    sysmoduleList->addItem(new tsl::elm::CategoryHeader("更新hekate配置  |  \uE0E0 切换", true));
    sysmoduleList->addItem(new tsl::elm::CustomDrawer([](tsl::gfx::Renderer *renderer, s32 x, s32 y, s32 w, s32 h) {
        renderer->drawString("\uE016  切换后重启生效。", false, x + 5, y + 20, 15, renderer->a(tsl::style::color::ColorDescription));
    }), 30);

    tsl::elm::ListItem *opAutoboot = new tsl::elm::ListItem("hekate自启动");
    Result rc{0};
    static std::string autobootValue = "-1";
    rc = setGetIniConfig("/bootloader/hekate_ipl.ini", "config", "autoboot", autobootValue);
    switch (rc)
	{
	case 1:
		opAutoboot->setValue("INI解析失败");
		break;
	case 2:
		opAutoboot->setValue("INI中无对应节");
		break;
	case 3:
		opAutoboot->setValue("INI节无参数");
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
        if (click & KEY_A) {
            Result rc{0};
            if (autobootValue == "1")
                autobootValue = "0";
            else if (autobootValue == "0")
                autobootValue = "1";
            else
                autobootValue = "-1";
            opAutoboot->setValue(autobootValue);
            rc = setGetIniConfig("/bootloader/hekate_ipl.ini", "config", "autoboot", autobootValue, false);
            switch (rc)
            {
            case 1:
                opAutoboot->setValue("INI解析失败");
                break;
            case 2:
                opAutoboot->setValue("INI中无对应节");
                break;
            case 3:
                opAutoboot->setValue("INI节无参数");
                break;
            case 4:
                opAutoboot->setValue("INI写入失败");
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

    sysmoduleList->addItem(new tsl::elm::CategoryHeader("大陆与国际版本切换  |  \uE0E2 大陆  \uE0E3 国际", true));
    sysmoduleList->addItem(new tsl::elm::CustomDrawer([](tsl::gfx::Renderer *renderer, s32 x, s32 y, s32 w, s32 h) {
        renderer->drawString("\uE016  切换后无需洗白和初始化，重启同意EULA即可。", false, x + 5, y + 20, 15, renderer->a(tsl::style::color::ColorDescription));
    }), 30);

    tsl::elm::ListItem *verSwitchItem = new tsl::elm::ListItem("当前版本");
    verSwitchItem->setValue(this->m_isTencentVersion ? "大陆" : "国际");
    verSwitchItem->setClickListener([this, verSwitchItem](u64 click) -> bool {
        Result rc{};
        if (click & KEY_X) {
            if (this->m_isTencentVersion) return true;
            if (R_FAILED(rc = setsysSetT(true))) {
                verSwitchItem->setText(std::string("setsysSetT失败！错误码：") + std::to_string(rc));
                return false;
            }
            if (R_FAILED(rc = setsysSetRegionCode(SetRegion_CHN))) {
                verSwitchItem->setText(std::string("setsysSetRegionCode失败！错误码：") + std::to_string(rc));
                return false;
            }
            this->m_isTencentVersion = true;
            verSwitchItem->setValue("大陆");
            return true;
        } else if (click & KEY_Y) {
            if (!this->m_isTencentVersion) return true;
            if (R_FAILED(rc = setsysSetT(false))) {
                verSwitchItem->setText(std::string("setsysSetT失败！错误码：") + std::to_string(rc));
                return false;
            }
            if (R_FAILED(rc = setsysSetRegionCode(SetRegion_HTK))) {
                verSwitchItem->setText(std::string("setsysSetRegionCode失败！错误码：") + std::to_string(rc));
                return false;
            }
            this->m_isTencentVersion = false;
            verSwitchItem->setValue("国际");
            return true;
        }
        return false;
    });
    sysmoduleList->addItem(verSwitchItem);

    rootFrame->setContent(sysmoduleList);

    return rootFrame;
}

Result GuiMain::setGetIniConfig(std::string iniPath, std::string iniSection, std::string iniOption, std::string &iniValue, bool getOption) {
    simpleIniParser::Ini *ini = simpleIniParser::Ini::parseFile(&this->m_fs, iniPath);
    if (!ini) return 1;
    simpleIniParser::IniSection *section = ini->findSection(iniSection);
    if (!section) return 2;
    simpleIniParser::IniOption *option = section->findFirstOption(iniOption);
    if (!option) return 3;

    if (getOption) {
        iniValue = option->value;
    } else {
        option->value = iniValue;
        if (!(ini->writeToFile(&this->m_fs, iniPath))) return 4;
    }

    return 0;
}

Result GuiMain::CopyFile(const char *srcPath, const char *destPath) {
	Result ret{0};

    FsFile src_handle, dest_handle;
	if (R_FAILED(ret = fsFsOpenFile(&this->m_fs, srcPath, FsOpenMode_Read, &src_handle))) return ret;
    tsl::hlp::ScopeGuard fileGuard1([&] { fsFileClose(&src_handle); });

	s64 size = 0;
	if (R_FAILED(ret = fsFileGetSize(&src_handle, &size))) return ret;

    if (R_SUCCEEDED(fsFsOpenFile(&this->m_fs, destPath, FsOpenMode_Read, &dest_handle))) {
        fsFileClose(&dest_handle);
        if (R_FAILED(ret = fsFsDeleteFile(&this->m_fs, destPath))) return ret;
	    if (R_FAILED(ret = fsFsCreateFile(&this->m_fs, destPath, size, 0))) return ret;
    }

	if (R_FAILED(ret = fsFsOpenFile(&this->m_fs, destPath, FsOpenMode_Write, &dest_handle))) return ret;
    tsl::hlp::ScopeGuard fileGuard2([&] { fsFileClose(&dest_handle); });

	u64 bytes_read = 0;
	const u64 buf_size = 0x10000;
	s64 offset = 0;
	unsigned char *buf = new unsigned char[buf_size];
    tsl::hlp::ScopeGuard fileGuard3([&] { delete[] buf; });
	std::string filename = std::filesystem::path(srcPath).filename();

	do {
		std::memset(buf, 0, buf_size);
		if (R_FAILED(ret = fsFileRead(&src_handle, offset, buf, buf_size, FsReadOption_None, &bytes_read))) return ret;
		if (R_FAILED(ret = fsFileWrite(&dest_handle, offset, buf, bytes_read, FsWriteOption_Flush))) return ret;
		offset += bytes_read;
	} while(offset < size);

	return ret;
}

void GuiMain::update() {
    static u32 counter = 0;

    if (counter++ % 20 != 0)
        return;

    for (const auto &module : this->m_sysmoduleListItems) {
        this->updateStatus(module);
    }

    this->m_listItem1->setValue((this->m_bootRunning == BootDatType::SXOS_BOOT_TYPE) ? "正在使用 | \uE0F4" : "未启用 | \uE098");
    this->m_listItem2->setValue((this->m_bootRunning == BootDatType::SXGEAR_BOOT_TYPE) ? "正在使用 | \uE0F4" : "未启用 | \uE098");
}

void GuiMain::updateStatus(const SystemModule &module) {
    bool running = this->isRunning(module);
    bool hasFlag = this->hasFlag(module);

    const char *desc = descriptions[running][hasFlag];
    module.listItem->setValue(desc);
}

bool GuiMain::hasFlag(const SystemModule &module) {
    FsFile flagFile;
    std::snprintf(pathBuffer, FS_MAX_PATH, boot2FlagFormat.c_str(), module.programId);
    Result rc = fsFsOpenFile(&this->m_fs, pathBuffer, FsOpenMode_Read, &flagFile);
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
