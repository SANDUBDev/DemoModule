#ifndef UnMIAKiwiH
#define UnMIAKiwiH

#include <System.Classes.hpp>
#include <Vcl.Controls.hpp>
#include <Vcl.StdCtrls.hpp>
#include <Vcl.Forms.hpp>
#include <Vcl.ExtCtrls.hpp>
#include <System.JSON.hpp>
#include <Winapi.WinINet.hpp>
#include "dtmod.h"
#include <Vcl.Buttons.hpp>
#include <Vcl.ComCtrls.hpp>
#include "dxGDIPlusClasses.hpp"
#include <Vcl.Imaging.pngimage.hpp>
#include "cxClasses.hpp"
#include "cxControls.hpp"
#include "cxGraphics.hpp"
#include "cxLookAndFeelPainters.hpp"
#include "cxLookAndFeels.hpp"
#include "dxGaugeCircularScale.hpp"
#include "dxGaugeControl.hpp"
#include "dxGaugeCustomScale.hpp"
#include "dxGaugeQuantitativeScale.hpp"
#include "cxContainer.hpp"
#include "cxEdit.hpp"
#include "cxImage.hpp"
#include <Data.Bind.Components.hpp>
#include <Data.Bind.ObjectScope.hpp>
#include <IPPeerClient.hpp>
#include <REST.Authenticator.Basic.hpp>
#include <REST.Client.hpp>
#include <REST.Authenticator.Simple.hpp>
#include <REST.Authenticator.OAuth.hpp>

enum EQRType
{
	eqrStatic,
	eqrDynamic,
	eqrHybridBase,
	eqrHybridExtension
};

class TPaymentInfo : public TObject
{
public:
	String TransactionID;
	double Amount;
	String Status;
	__fastcall TPaymentInfo() : Amount(0.0) {}
};

enum EIDType
{
	eidMerchant,
	eidExtension
};

struct TQRParams
{
	EQRType Type;
	double Amount;
	int LifetimeSeconds;
	String MerchantID;
	String BaseMerchantID;
	String Comment;
	TQRParams() : Type(eqrStatic), Amount(0.0), LifetimeSeconds(0) {}
};

struct MIAData
{
	String APIKey;
	String APISecret;
	String MerchantIBAN;
	String Token;
};

enum TLogLevel
{
	llInfo,
	llWarning,
	llError,
	llSuccess,
	llDebug
};

enum TLogSource
{
	lsUNA,
	lsFCB,
	lsQiwi,
	lsDatabase,
	lsSystem,
	lsUI
};

class TTMIAKiwi : public TForm
{
__published:
	TPanel *pnlMIAAll;
	TPageControl *pgcMIAPAyments;
	TTabSheet *tsPCQiwiMIA;
	TPanel *pnlTopMIAQiwi;
	TImage *imgTopLeftLogo;
	TImage *imgTopRightLogo;
	TLabel *lblSumToPay;
	TImage *imgQRToPay;
	TMemo *mmoInfoMIA;
	TProgressBar *pbTimeMIA;
	TPanel *pnlButRetry;
	TPanel *pnlButAnul;
	TImage *imgLogoRepeat;
	TLabel *lblTextRepeat;
	TImage *imgAnulLogo;
	TLabel *lblAnulText;
	TTabSheet *tsMIAGenQRandToken;
	TImage *imgQRQiwiToSee;
	TBitBtn *btnSaveImage;
	TBitBtn *btnCloseForm;
	TMemo *genSaveMemo;
	TTimer *tmrQr;
	TTabSheet *tsPCFCBMIA;
	TPanel *pnFCBMain;
	TPanel *Panel1;
	TImage *tLogoMIA;
	TImage *Image1;
	TPanel *pnSumToPay;
	TPanel *pnInfoSection;
	TPanel *pnQrImage;
	TPanel *pnTimerShow;
	TPanel *pnTextInfoPay;
	TPanel *pnTimeShow;
	TPanel *pnGraphicTimer;
	TImage *imgQR;
	TMemo *Memo1;
	TPanel *pnBtnRepeat;
	TImage *Image2;
	TPanel *pnTxtRepeat;
	TPanel *pnAnul;
	TImage *Image3;
	TPanel *pnTxtAnul;
	TPanel *pnInfoPay;
	TcxImage *cxImage1;
	TRESTClient *RESTClntMIA;
	TRESTRequest *RESTRqstMIA;
	TRESTResponse *RESTRspMIA;
	THTTPBasicAuthenticator *HTTPBAthMIA;
	TTabSheet *tsFCBGenToken;
	TPanel *pnGenFCBTop;
	TPanel *pnGenQR;
	TImage *FCBQrImage;
	TPanel *pnTopButControl;
	TPanel *pnButSaveQRFCB;
	TImage *pnButSaveImage;
	TPanel *pnButSaveText;
	TPanel *pnBackButFCBQRGen;
	TPanel *pnBtnGenQR;
	TImage *imgButGenQR;
	TPanel *pnButGenQRText;
	TPanel *pnBtnUseAdmin;
	TImage *imgBtnUseAdmin;
	TPanel *pnBtnUseAdminText;
	TPanel *pnAdminLoginPasw;
	TPanel *pnPassEdit;
	TImage *pnPassEdtImg;
	TEdit *edtPassword;
	TStaticText *stxtPass;
	TPanel *pnLoginEdit;
	TImage *pnLoginEdtImg;
	TEdit *edtLogin;
	TStaticText *stxtLogin;
	TPanel *pnBtnExit;
	TImage *imgButExit;
	TPanel *pnButText;
	TPanel *pnBtnDeviceSet;
	TImage *imgDeviceSet;
	TPanel *pnTextDeviceSet;
	TTimer *Timer1;
	TOAuth2Authenticator *t2authentificator;
	void __fastcall FormShow(TObject *Sender);
	void __fastcall btnSaveImageClick(TObject *Sender);
	void __fastcall btnCloseFormClick(TObject *Sender);
	void __fastcall tmrQrTimer(TObject *Sender);
	void __fastcall lblTextRepeatClick(TObject *Sender);
	void __fastcall imgLogoRepeatClick(TObject *Sender);
	void __fastcall lblAnulTextClick(TObject *Sender);
	void __fastcall imgAnulLogoClick(TObject *Sender);
	void __fastcall FormClose(TObject *Sender, TCloseAction &Action);
	void __fastcall pnBtnGenQRClick(TObject *Sender);
	void __fastcall pnBtnGenQRMouseDown(TObject *Sender, TMouseButton Button, TShiftState Shift, int X, int Y);
	void __fastcall pnBtnGenQRMouseUp(TObject *Sender, TMouseButton Button, TShiftState Shift, int X, int Y);
	void __fastcall pnBtnGenQRMouseEnter(TObject *Sender);
	void __fastcall pnBtnGenQRMouseLeave(TObject *Sender);
	void __fastcall pnBtnUseAdminClick(TObject *Sender);
	void __fastcall pnBtnUseAdminMouseDown(TObject *Sender, TMouseButton Button, TShiftState Shift, int X, int Y);
	void __fastcall pnBtnUseAdminMouseEnter(TObject *Sender);
	void __fastcall pnBtnUseAdminMouseLeave(TObject *Sender);
	void __fastcall pnBtnUseAdminMouseUp(TObject *Sender, TMouseButton Button, TShiftState Shift, int X, int Y);
	void __fastcall pnBtnExitClick(TObject *Sender);
	void __fastcall pnBtnExitMouseDown(TObject *Sender, TMouseButton Button, TShiftState Shift, int X, int Y);
	void __fastcall pnBtnExitMouseEnter(TObject *Sender);
	void __fastcall pnBtnExitMouseLeave(TObject *Sender);
	void __fastcall pnBtnExitMouseUp(TObject *Sender, TMouseButton Button, TShiftState Shift, int X, int Y);
	void __fastcall pnButSaveQRFCBClick(TObject *Sender);
	void __fastcall pnButSaveQRFCBMouseDown(TObject *Sender, TMouseButton Button, TShiftState Shift, int X, int Y);
	void __fastcall pnButSaveQRFCBMouseEnter(TObject *Sender);
	void __fastcall pnButSaveQRFCBMouseLeave(TObject *Sender);
	void __fastcall pnButSaveQRFCBMouseUp(TObject *Sender, TMouseButton Button, TShiftState Shift, int X, int Y);
	void __fastcall RESTRqstMIAAfterExecute(TCustomRESTRequest *Sender);
	void __fastcall pnBtnDeviceSetClick(TObject *Sender);
	void __fastcall pnBtnDeviceSetMouseEnter(TObject *Sender);
	void __fastcall pnBtnDeviceSetMouseLeave(TObject *Sender);
	void __fastcall pnBtnDeviceSetMouseUp(TObject *Sender, TMouseButton Button, TShiftState Shift, int X, int Y);
	void __fastcall pnBtnDeviceSetMouseDown(TObject *Sender, TMouseButton Button, TShiftState Shift, int X, int Y);
	void __fastcall mmoInfoMIAChange(TObject *Sender);
	void __fastcall pnBtnRepeatClick(TObject *Sender);
	void __fastcall pnBtnRepeatMouseDown(TObject *Sender, TMouseButton Button, TShiftState Shift, int X, int Y);
	void __fastcall pnBtnRepeatMouseEnter(TObject *Sender);
	void __fastcall pnBtnRepeatMouseLeave(TObject *Sender);
	void __fastcall pnBtnRepeatMouseUp(TObject *Sender, TMouseButton Button, TShiftState Shift, int X, int Y);
	void __fastcall pnAnulClick(TObject *Sender);
	void __fastcall pnAnulMouseDown(TObject *Sender, TMouseButton Button, TShiftState Shift, int X, int Y);
	void __fastcall pnAnulMouseEnter(TObject *Sender);
	void __fastcall pnAnulMouseLeave(TObject *Sender);
	void __fastcall pnAnulMouseUp(TObject *Sender, TMouseButton Button, TShiftState Shift, int X, int Y);
	void __fastcall Timer1Timer(TObject *Sender);

private:
	MIAData* FData;
	String FAuthToken;
	String FLastMerchantID;
	String FLastResultID;
	EIDType FLastResultIDType;
	String FLastProcessedStaticTxID;
	int valueTimer;
	AnsiString FMIAType;
	String FinstallationId;
	double currentSumToPay;

	void __fastcall savePassData(String instalationID, String terminalActivationId);
	void __fastcall verifyAndSetAPIData();
	void __fastcall SetOperationType(AnsiString NewValue);
	void __fastcall DecodeBase64ToImage(const String& base64, TImage* img);
	bool __fastcall CancelPaymentFCB();
	bool __fastcall CancelQrById(const String& qrId);

public:
	bool MIALicense;
	String FCurrentTerminalActivationId;
	String FAccessToken;
	String FRefreshToken;
	String FShiftId;
	String FQrId;
	String FExtensionId;
	bool genQR;
	double payAmount;

	void __fastcall sendQueryFCBMIA();
	void __fastcall confirmActivationWithOTP();
	void __fastcall regNewPC();
	String __fastcall genGUID();
	void __fastcall SetAuthHeader();
	void __fastcall SetAuthHeaders();
	bool __fastcall OpenShift();
	bool __fastcall CreateHybridQr();
	bool __fastcall FindActiveHybridQr();
	bool __fastcall CreatePaymentRequest(double AAmount);
	bool __fastcall SimulatePayment(double AAmount);
	String __fastcall CheckPaymentStatus();
	void __fastcall CloseShift();
	void __fastcall Logout();
	bool __fastcall Login(const String& AUsername, const String& APassword);
	void __fastcall ShowUserMessage(TLogSource source, TLogLevel level, const String& function, const String& message);

	__fastcall TTMIAKiwi(TComponent* Owner);
	__fastcall ~TTMIAKiwi();

	TPaymentInfo* CheckPayment(const String& id, EIDType idType);
	bool Authenticate();
	bool CancelPayment(const String& merchantID, EIDType idType);
	bool CancelCurrentOperation();
	bool CreateQR(const TQRParams& params);
	String CheckStatus(const String& id, EIDType idType);
	String CheckLastQRStatus();
	void __fastcall generateQR(int typeQR, double Amount = 0, String comment = "");
	void __fastcall Payment();
	void __fastcall LoadMIAQRImage();
	int __fastcall runMIA(int runType, double SumPayAmount = 0);

	__property AnsiString MIAType = {read = FMIAType, write = SetOperationType};
};

extern PACKAGE TTMIAKiwi *TMIAKiwi;

#endif
