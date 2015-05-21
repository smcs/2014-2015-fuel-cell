#ifndef THUNDERBALL_UI_PAGES_H
#define THUNDERBALL_UI_PAGES_H

enum UiScreen
{
	SplashScreen = 0,
	MainScreen,
	MainMenu,
	StatusMenu,
	ConfigMenu,
	TempScreen,
	FlowDutyCycleScreen,
	FlowSenseScreen,
	VoltagesScreen,
	NetworkConfigScreen,
	NetworkEnableScreen,
	RebootScreen,
	DeviceScreen,
	IpConfigScreen,
	SetIpAddrScreen,
	SetIpMaskScreen,
	SetIpGatewayScreen,
	SaveConfigScreen,
	UpgradeScreen,
	NetworkStatusScreen,
	ConditionsScreen,
	SystemMenu,
	ResetErrorScreen,
	BatteryConfigScreen,
	AppInfoScreen,
	FuelTempScreen,
	LoginScreen,
	BatteryChargeScreen,
	OpModeScreen,
	BatteryMenuScreen,
	BatteryTempConfigScreen,
	ChangePasswordInfoScreen,
	ChangePasswordLoginInScreen,
	ChangePasswordHelpScreen,
	EnterPasswordScreen,
	ConfirmPasswordScreen,
	ReEnterPasswordScreen,
	PasswordChangedScreen,
	PasswordNotChangedScreen,
	UsbConfigScreen,
	RebootDamageWarning,
	RebootTendingResetWarning,
	RemoteUiConfigScreen,
	UserMessageScreen,

	// Add new above here
	CommandMenu,
	FirstCommandScreen,
#if 0
	OverrideMenuScreen,
	CathodeConfigScreen,
	PbAirConfigScreen,
	PbFuelConfigScreen,
	AnodeAirConfigScreen,
	AnodeFuelConfigScreen,
	FanConfigScreen,
	ValveConfigScreen,
	IgniterConfigScreen,
#endif

	NumScreens,
};

class UiPageMessage;

class ThunderballUiPages
{
public:
	static UiPageMessage* GetMessagePage();
};

#endif