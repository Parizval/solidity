add_executable(solidity-upgrade
    solidity_upgrade/main.cpp
    solidity_upgrade/UpgradeChange.h
    solidity_upgrade/UpgradeChange.cpp
    solidity_upgrade/UpgradeSuite.h
    solidity_upgrade/Upgrade060.cpp
    solidity_upgrade/SolidityUpgrade.cpp
)
target_link_libraries(solidity-upgrade PRIVATE solidity Boost::boost Boost::program_options Boost::system)

include(GNUInstallDirs)
install(TARGETS solidity-upgrade DESTINATION "${CMAKE_INSTALL_BINDIR}")