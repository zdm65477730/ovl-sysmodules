#pragma once

#include <tesla.hpp>

struct SystemModule {
    tsl::elm::ListItem *listItem;
    u64 programId;
    bool needReboot;
};

enum class BootDatType {
    SXOS_BOOT_TYPE,
    SXGEAR_BOOT_TYPE
};

struct SystemModuleEnabledFlag {
    int powerControlEnabled{1};
    int wifiControlEnabled{1};
    int sysmodulesControlEnabled{1};
    int bootFileControlEnabled{1};
    int hekateRestartControlEnabled{1};
    int consoleRegionControlEnabled{1};
#if 0
    int wlanCountryCodeControlEnabled{1};
#endif
};

class GuiMain : public tsl::Gui {
  private:
    FsFileSystem m_fs;
    std::list<SystemModule> m_sysmoduleListItems;
    tsl::elm::ListItem *m_listItemSXOSBootType;
    tsl::elm::ListItem *m_listItemSXGEARBootType;
    tsl::elm::ListItem *m_listItemWifiSwitch;
    bool m_scanned;

  public:
    GuiMain();
    ~GuiMain();

    virtual tsl::elm::Element *createUI();
    virtual void update() override;

  private:
    void updateStatus(const SystemModule &module);
    bool hasFlag(const SystemModule &module);
    bool isRunning(const SystemModule &module);
    Result CopyFile(const char *srcPath, const char *destPath);
    Result setGetIniConfig(std::string iniPath, std::string iniSection, std::string iniOption, std::string &iniValue, bool getOption = true);
    Result isWifiOn(bool &isWifiOn);
  #if 0
    Result setWLANCountryCode(std::string wlanCountCode);
    Result getWLANCountryCode(std::string &outWlanCountCode);
  #endif

    SystemModuleEnabledFlag m_sysmodEnabledFlags;
    BootDatType m_bootRunning;
    bool m_isTencentVersion;
  #if 0
    std::string m_curCountryCode;
  #endif
};