#include <vcl.h>
#include <IdCoderMIME.hpp>
#include <Vcl.Imaging.pngimage.hpp>
#pragma hdrstop

#include "dtmod.h"
#include "UnMIAKiwi.h"
#include "UnLicenses.h"
#include "ucLogin.h"

#pragma package(smart_init)
#pragma link "dxGDIPlusClasses"
#pragma link "cxClasses"
#pragma link "cxControls"
#pragma link "cxGraphics"
#pragma link "cxLookAndFeelPainters"
#pragma link "cxLookAndFeels"
#pragma link "dxGaugeCircularScale"
#pragma link "dxGaugeControl"
#pragma link "dxGaugeCustomScale"
#pragma link "dxGaugeQuantitativeScale"
#pragma link "cxContainer"
#pragma link "cxEdit"
#pragma link "cxImage"
#pragma resource "*.dfm"

TTMIAKiwi *TMIAKiwi;

__fastcall TTMIAKiwi::TTMIAKiwi(TComponent* Owner)
	: TForm(Owner)
{
	FData = new MIAData();
	genQR = false;

	tLogoMIA->Picture = imgTopLeftLogo->Picture;
	imgButExit->Picture = Image3->Picture;

	SetOperationType(UAParams.ReadString("MIAType", ReadStrParam(rpmDBFirst, "MIAType", "")));

	pnButGenQRText->Caption = tutil::Lng("Genereaza QR", "Сгенерировать QR");
	pnButSaveText->Caption  = tutil::Lng("Salveaza QR", "Сохранить QR");
	pnBtnUseAdminText->Caption = tutil::Lng("User Admin", "Пользователь Админ");
	pnTextDeviceSet->Caption = tutil::Lng("Leaga dispozitiv", "Привязать устройство");
	stxtPass->Caption = tutil::Lng("Parola", "Пароль");
	stxtLogin->Caption = tutil::Lng("Login", "Логин");
	pnTextInfoPay->Caption = tutil::Lng("Se asteapta plata ...", "Ожидается оплата ...");
	pnInfoPay->Caption = tutil::Lng("Palta prin MIA", "Оплата MIA");
	pnTxtRepeat->Caption = tutil::Lng("Repeta", "Повторить");
	pnTxtAnul->Caption = tutil::Lng("Anuleaza", "Отменить");
	pnButText->Caption = tutil::Lng("Iesire", "Выход");

	String verifyString = UAParams.ReadString("GlobalDep"," ") + "-" + UAParams.ReadString("ServerID","") + "-" + DTM->PosID + "-MIA";

	if (MIAType == "FCB")
	{
		DTM->qsLicensesGet->ParamByName("GlobalDep")->AsString = UAParams.ReadString("GlobalDep"," ");
		DTM->qsLicensesGet->ParamByName("ServerID")->AsString = UAParams.ReadString("ServerID","");
		DTM->qsLicensesGet->ParamByName("PosID")->AsString = DTM->PosID;
		DTM->qsLicensesGet->ExecSQL();

		String key = "";
		while (!DTM->qsLicensesGet->Eof)
		{
			key = DTM->qsLicensesGet->FieldByName("LICENSE")->AsString;
			DTM->qsLicensesGet->Next();
		}

		if (UnFLicenses == NULL)
		{
			UnFLicenses = new TUnFLicenses(this);
		}
		if (MIAType == "FCB")
		{
			MIALicense = UnFLicenses->validateLiceses(verifyString, key);
		}
		else
		{
			MIALicense = true;
		}
	}
	if (MIAType != "FCB")
	{
		MIALicense = true;
	}

	if (!TMIAKiwi->MIAType.IsEmpty())
	{
		DTM->qsMIAVerifyCreateTokenTable->ExecSQL();
	}
}

__fastcall TTMIAKiwi::~TTMIAKiwi()
{
	delete FData;
}

bool TTMIAKiwi::Authenticate()
{
	mmoInfoMIA->Lines->Add("1. Получение токена...");
	HINTERNET hSession = NULL, hConnect = NULL, hRequest = NULL;
	bool success = false;
	try
	{
		TJSONObject* jsonPayload = new TJSONObject();
		jsonPayload->AddPair("apiKey", new TJSONString(FData->APIKey));
		jsonPayload->AddPair("apiSecret", new TJSONString(FData->APISecret));
		jsonPayload->AddPair("lifetimeMinutes", new TJSONNumber(1440));
		AnsiString postData = AnsiString(jsonPayload->ToString());
		delete jsonPayload;

		hSession = InternetOpen(L"C++ Builder App", INTERNET_OPEN_TYPE_DIRECT, NULL, NULL, 0);
		if (!hSession)
		{
			throw Exception("WinINet: InternetOpen failed");
		}
		hConnect = InternetConnect(hSession, L"api-stg.qiwi.md", INTERNET_DEFAULT_HTTPS_PORT, NULL, NULL, INTERNET_SERVICE_HTTP, 0, 0);
		if (!hConnect)
		{
			throw Exception("WinINet: InternetConnect failed");
		}
		hRequest = HttpOpenRequest(hConnect, L"POST", L"/v1/auth", NULL, NULL, NULL, INTERNET_FLAG_SECURE | INTERNET_FLAG_RELOAD, 0);
		if (!hRequest)
		{
			throw Exception("WinINet: HttpOpenRequest failed");
		}

		LPCWSTR headers = L"Content-Type: application/json\r\n";
		HttpSendRequest(hRequest, headers, wcslen(headers), (LPVOID)postData.c_str(), postData.Length());

		String responseText = "";
		char buffer[4096];
		DWORD bytesRead = 0;
		while (InternetReadFile(hRequest, buffer, sizeof(buffer) - 1, &bytesRead) && bytesRead > 0)
		{
			buffer[bytesRead] = '\0';
			responseText += String(buffer);
		}

		TJSONObject* jsonResponse = (TJSONObject*)TJSONObject::ParseJSONValue(responseText);
		if (jsonResponse)
		{
			try
			{
				String sToken = jsonResponse->GetValue("token")->Value();
				FData->Token = sToken;
				FAuthToken = "Bearer " + sToken;

				try
				{
					DTM->qsMIATokenInsert->ParamByName("PosID")->AsString = DTM->PosID;
					DTM->qsMIATokenInsert->ParamByName("Token")->AsString = sToken;
					DTM->qsMIATokenInsert->ExecSQL();
				}
				catch (Exception &e)
				{
					ShowUserMessage(lsUNA, llError, "Сохранение данных", "Данные синхронизированы");
				}

				mmoInfoMIA->Lines->Add("   [УСПЕХ] Токен получен и сохранен.");
				success = true;
			}
			__finally
			{
				delete jsonResponse;
			}
		}
		else
		{
			throw Exception("Не удалось получить токен из ответа: " + responseText);
		}
	}
	catch (const Exception &e)
	{
		mmoInfoMIA->Lines->Add("   [ОШИБКА] " + e.Message);
		success = false;
	}

	if (hRequest)
	{
		InternetCloseHandle(hRequest);
	}
	if (hConnect)
	{
		InternetCloseHandle(hConnect);
	}
	if (hSession)
	{
		InternetCloseHandle(hSession);
	}
	return success;
}

int __fastcall TTMIAKiwi::runMIA(int runType, double SumPayAmount)
{
	if (!MIALicense)
	{
		UnFLicenses->ShowModal();
		return 0;
	};

	if (TMIAKiwi->MIAType == "FCB")
	{
		verifyAndSetAPIData();
	}

	payAmount = SumPayAmount;
	if (runType == 1)
	{
		if (TMIAKiwi->MIAType == "Qiwi")
		{
			pgcMIAPAyments->ActivePage = tsMIAGenQRandToken;
			genQR = true;
		}
		else if (TMIAKiwi->MIAType == "FCB")
		{
			TMIAKiwi->Height = pnGenFCBTop->Height + pnGenFCBTop->Margins->Top + pnGenFCBTop->Margins->Bottom +
				pnBackButFCBQRGen->Height + pnBackButFCBQRGen->Margins->Top + pnBackButFCBQRGen->Margins->Bottom +
				pnBtnExit->Margins->Top + pnBtnExit->Margins->Bottom + pnBtnExit->Height +
				pnBtnDeviceSet->Margins->Top + pnBtnDeviceSet->Margins->Bottom + pnBtnDeviceSet->Height;

			pgcMIAPAyments->ActivePage = tsFCBGenToken;
			pnButSaveQRFCB->Enabled = false;
		}
	}
	else if (runType == 2)
	{
		if (TMIAKiwi->MIAType == "Qiwi")
		{
			pgcMIAPAyments->ActivePage = tsPCQiwiMIA;
		}
		else if (TMIAKiwi->MIAType == "FCB")
		{
			FloatToStr(SumPayAmount);
			currentSumToPay = SumPayAmount;
			CreatePaymentRequest(SumPayAmount);
			pgcMIAPAyments->ActivePage = tsPCFCBMIA;
		}
	}

	if (this->ShowModal() == mrOk)
	{
		return 1;
	}
	else
	{
		return 0;
	}
}

bool TTMIAKiwi::CreateQR(const TQRParams& params)
{
	if (FData->Token.IsEmpty())
	{
		genSaveMemo->Lines->Add("<<< Токен не получен. Выполните аутентификацию.");
		return false;
	}

	HINTERNET hSession = NULL, hConnect = NULL, hRequest = NULL;
	TJSONObject* jsonPayload = NULL;
	bool success = false;
	String endpoint;

	try
	{
		jsonPayload = new TJSONObject();
		String currentMerchantID = params.MerchantID;
		FLastResultIDType = eidMerchant;

		switch (params.Type)
		{
			case eqrStatic:
				endpoint = "/qr/create-qr-static";
				if (currentMerchantID.IsEmpty())
				{
					currentMerchantID = "LNM_STATIC_" + DTM->PosID;
				}
				jsonPayload->AddPair("amount", new TJSONNull());
				break;
			case eqrDynamic:
				endpoint = "/qr/create-qr-dynamic";
				if (params.Amount <= 0 || params.LifetimeSeconds < 30)
				{
					throw Exception("Для динамического QR сумма (>0) и срок жизни (>=30) обязательны.");
				}
				currentMerchantID = "LNM_DYN_" + FormatDateTime("yyyymmddhhnnsszzz", Now());
				jsonPayload->AddPair("amount", new TJSONNumber(params.Amount));
				jsonPayload->AddPair("validSeconds", new TJSONNumber(params.LifetimeSeconds));
				break;
			case eqrHybridBase:
				endpoint = "/qr/create-qr-hybrid";
				if (currentMerchantID.IsEmpty())
				{
					throw Exception("Для гибридной базы нужен постоянный MerchantID.");
				}
				break;
			case eqrHybridExtension:
				endpoint = "/qr/create-qr-extension";
				if (params.BaseMerchantID.IsEmpty() || params.Amount <= 0 || params.LifetimeSeconds < 30)
				{
					throw Exception("Для расширения гибрида нужны BaseMerchantID, сумма и срок жизни.");
				}
				currentMerchantID = params.BaseMerchantID;
				FLastResultIDType = eidExtension;
				jsonPayload->AddPair("amount", new TJSONNumber(params.Amount));
				jsonPayload->AddPair("validSeconds", new TJSONNumber(params.LifetimeSeconds));
				break;
		}

		FLastMerchantID = currentMerchantID;

		String commentStr = params.Comment.IsEmpty() ? String("Order payment") : params.Comment;
		String nameStr = "Payment " + FLastMerchantID;

		jsonPayload->AddPair("accountIBAN", new TJSONString(FData->MerchantIBAN));
		jsonPayload->AddPair("name", new TJSONString(nameStr));
		jsonPayload->AddPair("comment", new TJSONString(commentStr));
		jsonPayload->AddPair("merchantID", new TJSONString(currentMerchantID));
		jsonPayload->AddPair("redirectURL", new TJSONString("https://qiwi.md/"));

		String jsonString = jsonPayload->ToString();
		delete jsonPayload;
		jsonPayload = NULL;

		TBytes postDataBytes = TEncoding::UTF8->GetBytes(jsonString);

		UnicodeString headers_str = L"Content-Type: application/json; charset=utf-8\r\nAuthorization: " + FAuthToken + L"\r\n";
		genSaveMemo->Lines->Add("");
		genSaveMemo->Lines->Add("--- ОТПРАВКА ЗАПРОСА СОЗДАНИЯ QR ---");
		genSaveMemo->Lines->Add("Method: POST");
		genSaveMemo->Lines->Add("Endpoint: " + endpoint);
		genSaveMemo->Lines->Add("Headers: " + headers_str);
		genSaveMemo->Lines->Add("Body (UTF-8): " + jsonString);
		genSaveMemo->Lines->Add("------------------------------------");

		hSession = InternetOpen(L"C++ Builder App", INTERNET_OPEN_TYPE_DIRECT, NULL, NULL, 0);
		if (!hSession)
		{
			throw Exception("WinINet: InternetOpen failed");
		}
		hConnect = InternetConnect(hSession, L"api-stg.qiwi.md", INTERNET_DEFAULT_HTTPS_PORT, NULL, NULL, INTERNET_SERVICE_HTTP, 0, 0);
		if (!hConnect)
		{
			throw Exception("WinINet: InternetConnect failed");
		}
		hRequest = HttpOpenRequest(hConnect, L"POST", endpoint.c_str(), NULL, NULL, NULL, INTERNET_FLAG_SECURE | INTERNET_FLAG_RELOAD, 0);
		if (!hRequest)
		{
			throw Exception("WinINet: HttpOpenRequest failed");
		}

		HttpSendRequestW(hRequest, headers_str.c_str(), headers_str.Length(), (LPVOID)&postDataBytes[0], postDataBytes.Length);
		String responseText = "";
		char buffer[4096];
		DWORD bytesRead = 0;
		while (InternetReadFile(hRequest, buffer, sizeof(buffer) - 1, &bytesRead) && bytesRead > 0)
		{
			buffer[bytesRead] = '\0';
			responseText += String(buffer);
		}

		genSaveMemo->Lines->Add("--- ПОЛУЧЕН ОТВЕТ ОТ СЕРВЕРА ---");
		genSaveMemo->Lines->Add(responseText);
		genSaveMemo->Lines->Add("--------------------------------");

		TJSONObject* jsonResponse = (TJSONObject*)TJSONObject::ParseJSONValue(responseText);
		if (jsonResponse)
		{
			try
			{
				if (jsonResponse->Get("errors") != NULL || jsonResponse->Get("title") != NULL)
				{
					throw Exception("Сервер вернул ошибку: " + responseText);
				}

				TJSONPair* imagePair = jsonResponse->Get("image");
				if (imagePair != NULL && dynamic_cast<TJSONNull*>(imagePair->JsonValue) == NULL)
				{
					DecodeBase64ToImage(imagePair->JsonValue->Value(), imgQRQiwiToSee);
					DecodeBase64ToImage(imagePair->JsonValue->Value(), imgQRToPay);
				}
				else if (params.Type != eqrHybridExtension)
				{
					throw Exception("В ответе API отсутствует ожидаемое поле 'image'.");
				}

				if (FLastResultIDType == eidExtension)
				{
					TJSONPair* extPair = jsonResponse->Get("extensionUUID");
					if (extPair != NULL && dynamic_cast<TJSONNull*>(extPair->JsonValue) == NULL)
					{
						FLastResultID = extPair->JsonValue->Value();
					}
					else
					{
						throw Exception("В ответе API отсутствует ожидаемое поле 'extensionUUID'.");
					}
				}
				else
				{
					FLastResultID = currentMerchantID;
				}

				mmoInfoMIA->Lines->Add("   [УСПЕХ] QR создан. ID для проверки: " + FLastResultID);
				success = true;
			}
			__finally
			{
				delete jsonResponse;
			}
		}
		else
		{
			throw Exception("Ошибка разбора JSON ответа: " + responseText);
		}
	}
	catch (const Exception &e)
	{
		if (jsonPayload)
		{
			delete jsonPayload;
		}
		genSaveMemo->Lines->Add("   [ОШИБКА] " + e.Message);
		success = false;
	}

	if (hRequest)
	{
		InternetCloseHandle(hRequest);
	}
	if (hConnect)
	{
		InternetCloseHandle(hConnect);
	}
	if (hSession)
	{
		InternetCloseHandle(hSession);
	}

	return success;
}

TPaymentInfo* TTMIAKiwi::CheckPayment(const String& id, EIDType idType)
{
	if (FData->Token.IsEmpty())
	{
		mmoInfoMIA->Lines->Add("   [ОШИБКА] Нет токена");
		return NULL;
	}
	if (id.IsEmpty())
	{
		mmoInfoMIA->Lines->Add("   [ОШИБКА] Не указан ID для проверки");
		Authenticate();
		tmrQr->Enabled = false;
		Payment();
		return NULL;
	}

	String endpoint;
	if (idType == eidMerchant)
	{
		endpoint = "/qr/get-qr-status-by-merchant-id?merchantID=" + id + "&nExt=1";
	}
	else
	{
		endpoint = "/qr/get-qr-extension-status?extensionGuid=" + id + "&nTxs=1";
	}

	TPaymentInfo* resultInfo = NULL;
	HINTERNET hSession = NULL, hConnect = NULL, hRequest = NULL;
	try
	{
		hSession = InternetOpen(L"C++ Builder App", INTERNET_OPEN_TYPE_DIRECT, NULL, NULL, 0);
		if (!hSession)
		{
			throw;
		}
		hConnect = InternetConnect(hSession, L"api-stg.qiwi.md", INTERNET_DEFAULT_HTTPS_PORT, NULL, NULL, INTERNET_SERVICE_HTTP, 0, 0);
		if (!hConnect)
		{
			throw;
		}
		hRequest = HttpOpenRequest(hConnect, L"GET", endpoint.c_str(), NULL, NULL, NULL, INTERNET_FLAG_SECURE | INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE, 0);
		if (!hRequest)
		{
			throw;
		}

		UnicodeString headers_str = L"Authorization: " + FAuthToken + L"\r\n";
		HttpSendRequestW(hRequest, headers_str.c_str(), headers_str.Length(), NULL, 0);

		String responseText = "";
		char buffer[4096];
		DWORD bytesRead = 0;
		while (InternetReadFile(hRequest, buffer, sizeof(buffer) - 1, &bytesRead) && bytesRead > 0)
		{
			buffer[bytesRead] = '\0';
			responseText += String(buffer);
		}

		TJSONObject* jsonResponse = (TJSONObject*)TJSONObject::ParseJSONValue(responseText);
		Log(jsonResponse->ToString());

		if (jsonResponse)
		{
			try
			{
				if (jsonResponse->Get("errors") != NULL || jsonResponse->Get("title") != NULL)
				{
					throw Exception("Сервер вернул ошибку: " + responseText);
				}

				resultInfo = new TPaymentInfo();

				TJSONPair* paymentsPair = jsonResponse->Get("payments");
				TJSONArray* payments = paymentsPair ? (TJSONArray*)paymentsPair->JsonValue : NULL;

				if (payments && payments->Count > 0)
				{
					resultInfo->Status = "Paid";
					TJSONObject* lastPayment = (TJSONObject*)payments->Items[0];
					resultInfo->TransactionID = lastPayment->GetValue("reference")->Value();
					TJSONValue* amountValue = lastPayment->GetValue("amount");
					TJSONNumber* amountNumber = dynamic_cast<TJSONNumber*>(amountValue);
					if (amountNumber)
					{
						resultInfo->Amount = amountNumber->AsDouble;
					}
				}
				else
				{
					if (idType == eidMerchant)
					{
						TJSONPair* extensionsPair = jsonResponse->Get("extensions");
						TJSONArray* extensions = extensionsPair ? (TJSONArray*)extensionsPair->JsonValue : NULL;
						if (extensions && extensions->Count > 0)
						{
							resultInfo->Status = ((TJSONObject*)extensions->Items[0])->GetValue("extensionStatus")->Value();
						}
						else
						{
							resultInfo->Status = jsonResponse->GetValue("status")->Value();
						}
					}
					else
					{
						resultInfo->Status = jsonResponse->GetValue("extensionStatus")->Value();
					}
				}
			}
			__finally
			{
				delete jsonResponse;
			}
		}
	}
	catch (const Exception &e)
	{
		mmoInfoMIA->Lines->Add("   [ОШИБКА] в CheckPayment: " + e.Message);
		if (resultInfo)
		{
			delete resultInfo;
		}
		resultInfo = NULL;
	}

	if (hRequest)
	{
		InternetCloseHandle(hRequest);
	}
	if (hConnect)
	{
		InternetCloseHandle(hConnect);
	}
	if (hSession)
	{
		InternetCloseHandle(hSession);
	}

	return resultInfo;
}

String TTMIAKiwi::CheckStatus(const String& id, EIDType idType)
{
	if (FData->Token.IsEmpty())
	{
		return "Error: No token";
	}
	if (id.IsEmpty())
	{
		return "Error: ID is empty";
	}

	String endpoint;
	String finalStatus = "Unknown";

	if (idType == eidMerchant)
	{
		endpoint = "/qr/get-qr-status-by-merchant-id?merchantID=" + id + "&nExt=1";
	}
	else
	{
		endpoint = "/qr/get-qr-extension-status?extensionGuid=" + id + "&nTxs=1";
	}

	HINTERNET hSession = NULL, hConnect = NULL, hRequest = NULL;
	try
	{
		UnicodeString headers_str = L"Authorization: " + FAuthToken + L"\r\n";
		mmoInfoMIA->Lines->Add("");
		mmoInfoMIA->Lines->Add("--- ОТПРАВКА ЗАПРОСА ПРОВЕРКИ СТАТУСА ---");
		mmoInfoMIA->Lines->Add("Method: GET");
		mmoInfoMIA->Lines->Add("Endpoint: " + endpoint);
		mmoInfoMIA->Lines->Add("Headers: " + headers_str);
		mmoInfoMIA->Lines->Add("-----------------------------------------");

		hSession = InternetOpen(L"C++ Builder App", INTERNET_OPEN_TYPE_DIRECT, NULL, NULL, 0);
		if (!hSession)
		{
			throw Exception("WinINet: InternetOpen failed");
		}
		hConnect = InternetConnect(hSession, L"api-stg.qiwi.md", INTERNET_DEFAULT_HTTPS_PORT, NULL, NULL, INTERNET_SERVICE_HTTP, 0, 0);
		if (!hConnect)
		{
			throw Exception("WinINet: InternetConnect failed");
		}
		hRequest = HttpOpenRequest(hConnect, L"GET", endpoint.c_str(), NULL, NULL, NULL, INTERNET_FLAG_SECURE | INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE, 0);
		if (!hRequest)
		{
			throw Exception("WinINet: HttpOpenRequest failed");
		}

		if (!HttpSendRequestW(hRequest, headers_str.c_str(), headers_str.Length(), NULL, 0))
		{
			throw Exception("WinINet: HttpSendRequest failed. Error: " + IntToStr((int)GetLastError()));
		}

		String responseText = "";
		char buffer[4096];
		DWORD bytesRead = 0;
		while (InternetReadFile(hRequest, buffer, sizeof(buffer) - 1, &bytesRead) && bytesRead > 0)
		{
			buffer[bytesRead] = '\0';
			responseText += String(buffer);
		}

		mmoInfoMIA->Lines->Add("--- ПОЛУЧЕН ОТВЕТ ОТ СЕРВЕРА ---");
		mmoInfoMIA->Lines->Add(responseText);
		mmoInfoMIA->Lines->Add("--------------------------------");

		TJSONObject* jsonResponse = (TJSONObject*)TJSONObject::ParseJSONValue(responseText);
		if (jsonResponse)
		{
			try
			{
				if (jsonResponse->Get("errors") != NULL || jsonResponse->Get("title") != NULL)
				{
					throw Exception("Сервер вернул ошибку: " + responseText);
				}

				TJSONPair* paymentsPair = jsonResponse->Get("payments");
				TJSONArray* payments = paymentsPair ? (TJSONArray*)paymentsPair->JsonValue : NULL;

				if (payments && payments->Count > 0)
				{
					finalStatus = "Paid";
				}
				else
				{
					if (idType == eidMerchant)
					{
						TJSONPair* extensionsPair = jsonResponse->Get("extensions");
						TJSONArray* extensions = extensionsPair ? (TJSONArray*)extensionsPair->JsonValue : NULL;
						if (extensions && extensions->Count > 0)
						{
							finalStatus = ((TJSONObject*)extensions->Items[0])->GetValue("extensionStatus")->Value();
						}
						else
						{
							finalStatus = jsonResponse->GetValue("status")->Value();
						}
					}
					else
					{
						finalStatus = jsonResponse->GetValue("extensionStatus")->Value();
					}
				}
			}
			__finally
			{
				delete jsonResponse;
			}
		}
		else
		{
			wchar_t statusCode[4];
			DWORD statusCodeSize = sizeof(statusCode);
			if (HttpQueryInfoW(hRequest, HTTP_QUERY_STATUS_CODE, &statusCode, &statusCodeSize, NULL) && wcscmp(statusCode, L"404") == 0)
			{
				finalStatus = "Active";
			}
			else
			{
				throw Exception("Ошибка разбора JSON ответа: " + responseText);
			}
		}
	}
	catch (const Exception &e)
	{
		mmoInfoMIA->Lines->Add("   [ОШИБКА] " + e.Message);
		return "Error";
	}

	if (hRequest)
	{
		InternetCloseHandle(hRequest);
	}
	if (hConnect)
	{
		InternetCloseHandle(hConnect);
	}
	if (hSession)
	{
		InternetCloseHandle(hSession);
	}

	mmoInfoMIA->Lines->Add("   Итоговый статус: " + finalStatus);
	return finalStatus;
}

String TTMIAKiwi::CheckLastQRStatus()
{
	if (FLastResultID.IsEmpty())
	{
		mmoInfoMIA->Lines->Add("Нет последнего созданного QR для проверки.");
		return "Not Created";
	}
	return CheckStatus(FLastResultID, FLastResultIDType);
}

void __fastcall TTMIAKiwi::DecodeBase64ToImage(const String& base64, TImage* img)
{
	if (!img)
	{
		return;
	}
	TMemoryStream* DecodedStream = new TMemoryStream();
	try
	{
		TIdDecoderMIME *decoder = new TIdDecoderMIME(NULL);
		try
		{
			decoder->DecodeStream(base64, DecodedStream);
			DecodedStream->Position = 0;
			if (DecodedStream->Size > 0)
			{
				TPngImage* png = new TPngImage();
				try
				{
					png->LoadFromStream(DecodedStream);
					img->Picture->Assign(png);
				}
				__finally
				{
					delete png;
				}
			}
		}
		__finally
		{
			delete decoder;
		}
	}
	__finally
	{
		delete DecodedStream;
	}
}

void __fastcall TTMIAKiwi::Payment()
{
	String MIAWorkType = UAParams.ReadString("MIAWorkType", ReadStrParam(rpmDBFirst, "MIAWorkType", ""));
	lblAnulText->Caption = "Anuleaza";
	lblSumToPay->Caption = FloatToStr(payAmount) + " MDL";
	TQRParams params;
	if (MIAWorkType == "Static")
	{
		LoadMIAQRImage();
		valueTimer = UAParams.ReadInteger("MIAPayTimeOut", ReadIntParam(rpmDBFirst, "MIAPayTimeOut", 100));
		tmrQr->Enabled = true;
		pbTimeMIA->Max = valueTimer;
	}
	else if (MIAWorkType == "Hybrid")
	{
		params.Type = eqrHybridExtension;
		params.Amount = payAmount;
		params.LifetimeSeconds = UAParams.ReadInteger("MIAPayTimeOut", ReadIntParam(rpmDBFirst, "MIAPayTimeOut", 100));
		params.Comment = "Achitare cec";
		params.BaseMerchantID = "DEIG_" + IntToStr(DTM->PosID);

		if (CreateQR(params))
		{
			mmoInfoMIA->Lines->Add("Счет на " + FloatToStr(payAmount) + " лей активирован!\nКлиент может сканировать основной QR-код для оплаты.");
		}
		else
		{
			mmoInfoMIA->Lines->Add("Не удалось активировать счет.");
		}
		LoadMIAQRImage();
		valueTimer = UAParams.ReadInteger("MIAPayTimeOut", ReadIntParam(rpmDBFirst, "MIAPayTimeOut", 100));
		tmrQr->Enabled = true;
		pbTimeMIA->Max = valueTimer;
	}
	else if (MIAWorkType == "Dynamic")
	{
		params.Type = eqrDynamic;
		params.Amount = payAmount;
		params.LifetimeSeconds = UAParams.ReadInteger("MIAPayTimeOut", ReadIntParam(rpmDBFirst, "MIAPayTimeOut", 100));
		CreateQR(params);
		valueTimer = UAParams.ReadInteger("MIAPayTimeOut", ReadIntParam(rpmDBFirst, "MIAPayTimeOut", 100));
		tmrQr->Enabled = true;
		pbTimeMIA->Max = valueTimer;
	}
}

void __fastcall TTMIAKiwi::LoadMIAQRImage()
{
	String filePath = ExtractFilePath(Application->ExeName) + "miaQR.png";
	if (FileExists(filePath))
	{
		try
		{
			if (TMIAKiwi->MIAType == "Qiwi")
			{
				imgQRToPay->Picture->LoadFromFile(filePath);
			}
			else if (TMIAKiwi->MIAType == "FCB")
			{
				imgQR->Picture->LoadFromFile(filePath);
			}
			mmoInfoMIA->Lines->Add("Изображение MIAQR.png успешно загружено.");
		}
		catch (const Exception &e)
		{
			mmoInfoMIA->Lines->Add("Ошибка при загрузке MIAQR.png: " + e.Message);
		}
	}
	else
	{
		mmoInfoMIA->Lines->Add("Файл MIAQR.png не найден в папке программы.");
	}
}

void __fastcall TTMIAKiwi::FormShow(TObject *Sender)
{
	if (TMIAKiwi->MIAType == "Qiwi")
	{
		valueTimer = ReadIntParam(rpmDBFirst, "MIAPayTimeOut", 100);
		pnTimeShow->Caption = ReadStrParam(rpmDBFirst, "MIAPayTimeOut", "100");
		pnButSaveQRFCB->Enabled = false;

		String MIAWorkType = UAParams.ReadString("MIAWorkType", ReadStrParam(rpmDBFirst, "MIAWorkType", ""));
		lblAnulText->Caption = "Inchide";
		genSaveMemo->Lines->Clear();
		mmoInfoMIA->Lines->Clear();
		FData->APIKey = UAParams.ReadString("MIAApiKey", ReadStrParam(rpmDBFirst, "MIAApiKey", ""));
		FData->APISecret = UAParams.ReadString("MIAApiSecrect", ReadStrParam(rpmDBFirst, "MIAApiSecrect", ""));
		FData->MerchantIBAN = UAParams.ReadString("MIAApiIBAN", ReadStrParam(rpmDBFirst, "MIAApiIBAN", ""));

		if (genQR)
		{
			genQR = false;
			if (MIAWorkType == "Static")
			{
				generateQR(0);
			}
			else if (MIAWorkType == "Hybrid")
			{
				generateQR(1);
			}
			else if (MIAWorkType == "Dynamic")
			{
				generateQR(2);
			}
		}
		else
		{
			Payment();
		}
	}
}

void __fastcall TTMIAKiwi::generateQR(int typeQR, double Amount, String comment)
{
	TQRParams params;

	switch (typeQR)
	{
		case 0:
		{
			genSaveMemo->Lines->Add("--- Создание статического QR кода ---");
			params.Type = eqrStatic;
			params.MerchantID = "LNM_STATIC_" + IntToStr(DTM->PosID);
			CreateQR(params);
			String staticMerchantID = FLastMerchantID;
			if (staticMerchantID.IsEmpty())
			{
				genSaveMemo->Lines->Add("Не удалось создать статический QR-код.");
			}
			break;
		}
		case 1:
		{
			genSaveMemo->Lines->Add("--- Создание базового гибридного QR ---");
			params.Type = eqrHybridBase;
			params.MerchantID = "DEIG_" + IntToStr(DTM->PosID);
			if (!CreateQR(params))
			{
				genSaveMemo->Lines->Add("Не удалось создать базовый гибридный QR.");
			}
			break;
		}
		case 2:
		{
			mmoInfoMIA->Lines->Add("--- Создание базового гибридного QR ---");
			params.Type = eqrDynamic;
			params.Amount = Amount;
			params.LifetimeSeconds = UAParams.ReadInteger("MIAPayTimeOut", ReadIntParam(rpmDBFirst, "MIAPayTimeOut", 100));
			params.Comment = comment;
			CreateQR(params);
			String dynamicMerchantID = FLastMerchantID;
			if (!dynamicMerchantID.IsEmpty())
			{
				mmoInfoMIA->Lines->Add("QR-код для оплаты создан. ID: " + dynamicMerchantID);
			}
			else
			{
				mmoInfoMIA->Lines->Add("Не удалось создать динамический QR-код.");
			}
			break;
		}
	}
}

void __fastcall TTMIAKiwi::btnSaveImageClick(TObject *Sender)
{
	imgQRQiwiToSee->Picture->SaveToFile("MIAQR.png");
	ShowUserMessage(lsUNA, llInfo, "", "Poza a fost salvata ca: MIAQR.png in folder cu program");
}

void __fastcall TTMIAKiwi::btnCloseFormClick(TObject *Sender)
{
	this->Close();
}

void __fastcall TTMIAKiwi::tmrQrTimer(TObject *Sender)
{
	pbTimeMIA->Position = valueTimer;
	TPaymentInfo* payment;
	String MIAWorkType = UAParams.ReadString("MIAWorkType", ReadStrParam(rpmDBFirst, "MIAWorkType", ""));
	mmoInfoMIA->Lines->Add(">> Проверка состояния оплаты");

	if (TMIAKiwi->MIAType == "Qiwi")
	{
		if (MIAWorkType == "Static")
		{
			payment = CheckPayment("LNM_STATIC_" + IntToStr(DTM->PosID), eidMerchant);
			if (payment)
			{
				if (payment->Status == "Paid")
				{
					if (payment->TransactionID != FLastProcessedStaticTxID && !payment->TransactionID.IsEmpty())
					{
						mmoInfoMIA->Lines->Add("ПОЛУЧЕН НОВЫЙ ПЛАТЕЖ!\nID: " + payment->TransactionID + "\nСумма: " + FloatToStr(payment->Amount));
						FLastProcessedStaticTxID = payment->TransactionID;
					}
					else
					{
						mmoInfoMIA->Lines->Add("Новых платежей нет. Последний известный платеж был на сумму " + FloatToStr(payment->Amount));
					}
				}
				else
				{
					mmoInfoMIA->Lines->Add("Новых платежей нет. QR-код активен (статус: " + payment->Status + ")");
				}
				delete payment;
			}
			else
			{
				mmoInfoMIA->Lines->Add("Не удалось получить информацию о QR.");
			}
		}
		else if (MIAWorkType == "Hybrid")
		{
			payment = CheckPayment(FLastResultID, FLastResultIDType);
			if (payment)
			{
				if (payment->Status == "Paid")
				{
					mmoInfoMIA->Lines->Add("Успех! Платеж по расширению получен. Сумма: " + FloatToStr(payment->Amount));
					FLastResultID = "";
					tmrQr->Enabled = false;
					Sleep(1000);
					ModalResult = mrOk;
				}
				delete payment;
			}
			else
			{
				mmoInfoMIA->Lines->Add("Не удалось получить информацию о расширении.");
			}
		}
		else if (MIAWorkType == "Dynamic")
		{
			payment = CheckPayment(FLastResultID, FLastResultIDType);
			if (payment)
			{
				if (payment->Status == "Paid")
				{
					mmoInfoMIA->Lines->Add("Успех! Платеж на сумму " + FloatToStr(payment->Amount) + " получен.");
					ModalResult = mrOk;
					return;
				}
				delete payment;
			}
			else
			{
				mmoInfoMIA->Lines->Add("Не удалось получить информацию о платеже.");
			}
		}
	}

	if (TMIAKiwi->MIAType == "FCB")
	{
		if (CheckPaymentStatus() == "paid")
		{
			mmoInfoMIA->Lines->Add("Успех! Платеж получен.");
			tmrQr->Enabled = false;
			ModalResult = mrOk;
			return;
		}
	}

	valueTimer -= tmrQr->Interval / 1000;
	pnTimeShow->Caption = IntToStr(valueTimer);

	if (valueTimer <= 0)
	{
		tmrQr->Enabled = false;
		mmoInfoMIA->Lines->Add("Время ожидания истекло.");
		pnlButRetry->Enabled = true;
		pbTimeMIA->Position = 0;
		pnTextInfoPay->Caption = "Время ожидания истекло.";
		pnBtnRepeat->Enabled = true;
	}
	else
	{
		pbTimeMIA->Position = valueTimer;
		pnTimeShow->Caption = IntToStr(valueTimer);
	}
}

void __fastcall TTMIAKiwi::lblTextRepeatClick(TObject *Sender)
{
	Payment();
}

void __fastcall TTMIAKiwi::imgLogoRepeatClick(TObject *Sender)
{
	Payment();
}

bool TTMIAKiwi::CancelPayment(const String& merchantID, EIDType idType)
{
	if (FData->Token.IsEmpty())
	{
		mmoInfoMIA->Lines->Add("   [ОШИБКА ОТМЕНЫ] Нет токена");
		return false;
	}
	if (merchantID.IsEmpty())
	{
		mmoInfoMIA->Lines->Add("   [ОШИБКА ОТМЕНЫ] Не указан ID для отмены");
		return false;
	}

	String endpoint;
	if (idType == eidExtension)
	{
		endpoint = "/qr/cancel-qr-extension?merchantID=" + merchantID;
	}
	else
	{
		endpoint = "/qr/cancel-qr?merchantID=" + merchantID;
	}

	mmoInfoMIA->Lines->Add(">> Попытка отмены. ID: " + merchantID + ", Endpoint: " + endpoint);

	bool success = false;
	HINTERNET hSession = NULL, hConnect = NULL, hRequest = NULL;
	try
	{
		hSession = InternetOpen(L"C++ Builder App", INTERNET_OPEN_TYPE_DIRECT, NULL, NULL, 0);
		if (!hSession)
		{
			throw Exception("WinINet: InternetOpen failed");
		}
		hConnect = InternetConnect(hSession, L"api-stg.qiwi.md", INTERNET_DEFAULT_HTTPS_PORT, NULL, NULL, INTERNET_SERVICE_HTTP, 0, 0);
		if (!hConnect)
		{
			throw Exception("WinINet: InternetConnect failed");
		}
		hRequest = HttpOpenRequest(hConnect, L"POST", endpoint.c_str(), NULL, NULL, NULL, INTERNET_FLAG_SECURE | INTERNET_FLAG_RELOAD, 0);
		if (!hRequest)
		{
			throw Exception("WinINet: HttpOpenRequest failed");
		}

		UnicodeString headers_str = L"Authorization: " + FAuthToken + L"\r\n";
		if (!HttpSendRequestW(hRequest, headers_str.c_str(), headers_str.Length(), NULL, 0))
		{
			throw Exception("WinINet: HttpSendRequest failed. Error: " + IntToStr((int)GetLastError()));
		}

		wchar_t statusCode[4];
		DWORD statusCodeSize = sizeof(statusCode);
		if (HttpQueryInfoW(hRequest, HTTP_QUERY_STATUS_CODE, &statusCode, &statusCodeSize, NULL))
		{
			if (wcscmp(statusCode, L"200") == 0)
			{
				mmoInfoMIA->Lines->Add("   [УСПЕХ] Операция успешно отменена на сервере.");
				success = true;
			}
			else
			{
				String responseText = "";
				char buffer[1024];
				DWORD bytesRead = 0;
				while (InternetReadFile(hRequest, buffer, sizeof(buffer) - 1, &bytesRead) && bytesRead > 0)
				{
					buffer[bytesRead] = '\0';
					responseText += String(buffer);
				}
				throw Exception("Сервер вернул код " + String(statusCode) + ". Ответ: " + responseText);
			}
		}
		else
		{
			throw Exception("Не удалось получить код состояния HTTP.");
		}
	}
	catch (const Exception &e)
	{
		mmoInfoMIA->Lines->Add("   [ОШИБКА ОТМЕНЫ] " + e.Message);
		success = false;
	}

	if (hRequest)
	{
		InternetCloseHandle(hRequest);
	}
	if (hConnect)
	{
		InternetCloseHandle(hConnect);
	}
	if (hSession)
	{
		InternetCloseHandle(hSession);
	}

	return success;
}

bool TTMIAKiwi::CancelCurrentOperation()
{
	mmoInfoMIA->Lines->Add(">> Запрос на универсальную отмену операции...");

	String MIAWorkType = UAParams.ReadString("MIAWorkType", ReadStrParam(rpmDBFirst, "MIAWorkType", ""));
	int qrType;
	if (MIAWorkType == "Static")
	{
		qrType = 0;
	}
	else if (MIAWorkType == "Hybrid")
	{
		qrType = 1;
	}
	else if (MIAWorkType == "Dynamic")
	{
		qrType = 2;
	}

	switch (qrType)
	{
		case 0:
		{
			mmoInfoMIA->Lines->Add("   [ИНФО] Отмена не требуется для статического QR-кода. Операция пропущена.");
			return true;
		}
		case 1:
		{
			mmoInfoMIA->Lines->Add("   [ИНФО] Отмена расширения для гибридного QR...");
			if (FLastMerchantID.IsEmpty())
			{
				mmoInfoMIA->Lines->Add("   [ОШИБКА] Не найден ID базового QR для отмены расширения.");
				return false;
			}
			return CancelPayment(FLastMerchantID, eidExtension);
		}
		case 2:
		{
			mmoInfoMIA->Lines->Add("   [ИНФО] Отмена динамического QR-кода...");
			if (FLastMerchantID.IsEmpty())
			{
				mmoInfoMIA->Lines->Add("   [ОШИБКА] Не найден ID динамического QR для отмены.");
				return false;
			}
			return CancelPayment(FLastMerchantID, eidMerchant);
		}
		default:
		{
			mmoInfoMIA->Lines->Add("   [ОШИБКА] Неизвестный тип QR для отмены: " + IntToStr(qrType));
			return false;
		}
	}
}

void __fastcall TTMIAKiwi::lblAnulTextClick(TObject *Sender)
{
	if (tmrQr->Enabled)
	{
		CancelCurrentOperation();
		tmrQr->Enabled = false;
		lblAnulText->Caption = "Inchide";
	}
	else
	{
		this->Close();
	}
}

void __fastcall TTMIAKiwi::imgAnulLogoClick(TObject *Sender)
{
	if (tmrQr->Enabled)
	{
		CancelCurrentOperation();
		tmrQr->Enabled = false;
		lblAnulText->Caption = "Inchide";
	}
	else
	{
		ModalResult = mrCancel;
	}
}

void __fastcall TTMIAKiwi::sendQueryFCBMIA()
{
	if (tmrQr->Enabled)
	{
		CancelCurrentOperation();
		tmrQr->Enabled = false;
		lblAnulText->Caption = "Inchide";
	}
	else
	{
		ModalResult = mrCancel;
	}
}

void __fastcall TTMIAKiwi::FormClose(TObject *Sender, TCloseAction &Action)
{
	TMIAKiwi->Height = 585;
	pnAdminLoginPasw->Visible = false;
	edtLogin->Text = "";
	edtPassword->Text = "";
	pnSumToPay->Caption = "0.00 MDL";
	pnTextInfoPay->Caption = "Ожидается оплата";
}

void __fastcall TTMIAKiwi::pnBtnGenQRClick(TObject *Sender)
{
	String qrUserName;
	String qrPassword;
	UnicodeString curUserName = "123";
	UnicodeString curPassword = "123";

	qrUserName = edtLogin->Text.IsEmpty() ? curUserName : edtLogin->Text;
	qrPassword = edtPassword->Text.IsEmpty() ? curPassword : edtPassword->Text;

	CreateHybridQr();
}

void __fastcall TTMIAKiwi::pnBtnGenQRMouseDown(TObject *Sender, TMouseButton Button, TShiftState Shift, int X, int Y)
{
	pnBtnGenQR->BevelKind = Vcl::Controls::bkNone;
	pnBtnGenQR->BevelInner = Vcl::Controls::bvNone;
}

void __fastcall TTMIAKiwi::pnBtnGenQRMouseUp(TObject *Sender, TMouseButton Button, TShiftState Shift, int X, int Y)
{
	pnBtnGenQR->BevelKind = Vcl::Controls::bkTile;
	pnBtnGenQR->BevelInner = Vcl::Controls::bvRaised;
}

void __fastcall TTMIAKiwi::pnBtnGenQRMouseEnter(TObject *Sender)
{
	pnButGenQRText->Font->Color = clScrollBar;
}

void __fastcall TTMIAKiwi::pnBtnGenQRMouseLeave(TObject *Sender)
{
	pnButGenQRText->Font->Color = clWindowText;
}

void __fastcall TTMIAKiwi::pnBtnUseAdminClick(TObject *Sender)
{
	pnAdminLoginPasw->Visible = !pnAdminLoginPasw->Visible;
}

void __fastcall TTMIAKiwi::pnBtnUseAdminMouseDown(TObject *Sender, TMouseButton Button, TShiftState Shift, int X, int Y)
{
	pnBtnUseAdmin->BevelKind = Vcl::Controls::bkNone;
	pnBtnUseAdmin->BevelInner = Vcl::Controls::bvNone;
}

void __fastcall TTMIAKiwi::pnBtnUseAdminMouseEnter(TObject *Sender)
{
	pnBtnUseAdminText->Font->Color = clScrollBar;
}

void __fastcall TTMIAKiwi::pnBtnUseAdminMouseLeave(TObject *Sender)
{
	pnBtnUseAdminText->Font->Color = clWindowText;
}

void __fastcall TTMIAKiwi::pnBtnUseAdminMouseUp(TObject *Sender, TMouseButton Button, TShiftState Shift, int X, int Y)
{
	pnBtnUseAdmin->BevelKind = Vcl::Controls::bkTile;
	pnBtnUseAdmin->BevelInner = Vcl::Controls::bvRaised;
}

void __fastcall TTMIAKiwi::regNewPC()
{
	String terminalID = ReadStrParam(rpmDBFirst, "MIATerminalID", "");
	String merchantIdno = ReadStrParam(rpmDBFirst, "MIAmerchantIdno", "");
	String installationId = genGUID();
	if (installationId.IsEmpty())
	{
		ShowUserMessage(lsUNA, llError, "Регистрация оборудывания", "Не удалось получить Installation ID. Регистрация невозможна.");
		return;
	}
	FinstallationId = installationId;

	RESTClntMIA->BaseURL = ReadStrParam(rpmDBFirst, "MIAAPiURL", "http://localhost:5000/pos/api/");
	if (!RESTClntMIA->BaseURL.IsEmpty() && RESTClntMIA->BaseURL[RESTClntMIA->BaseURL.Length()] != '/')
	{
		RESTClntMIA->BaseURL = RESTClntMIA->BaseURL + "/";
	}
	RESTRqstMIA->Resource = "v1/terminal-activation/init";
	RESTRqstMIA->Method = TRESTRequestMethod::rmPOST;
	RESTRqstMIA->Params->Clear();
	RESTRqstMIA->ClearBody();
	RESTRqstMIA->Params->AddHeader("X-INSTALLATION-ID", FinstallationId);
	RESTRqstMIA->Params->AddHeader("X-DEVICE", "UNA POS");
	RESTRqstMIA->Params->AddHeader("X-PLATFORM-TYPE", "web");
	RESTRqstMIA->Params->AddHeader("X-POS-APP-VERSION", "1");

	String jsonStringBody = "{\"terminalId\": \"" + terminalID + "\", \"merchantIdno\": \"" + merchantIdno + "\"}";
	RESTRqstMIA->Body->Add(jsonStringBody, TRESTContentType::ctAPPLICATION_JSON);

	try
	{
		RESTRqstMIA->Execute();

		if (RESTRspMIA->StatusCode == 200)
		{
			TJSONObject* jsonResponse = (TJSONObject*)TJSONObject::ParseJSONValue(RESTRspMIA->Content);
			if (jsonResponse)
			{
				String terminalActivationId = jsonResponse->GetValue("terminalActivationId")->Value();
				String maskedPhone = jsonResponse->GetValue("maskedPhoneNumber")->Value();

				this->FCurrentTerminalActivationId = terminalActivationId;

				ShowUserMessage(lsFCB, llInfo, "Регистрация оборудывания", "На номер " + maskedPhone + " отправлен код подтверждения.");
				confirmActivationWithOTP();

				savePassData(FinstallationId, terminalActivationId);
				delete jsonResponse;
			}
		}
		else
		{
			ShowUserMessage(lsFCB, llError, "Регистрация оборудывания", "Ошибка сервера. Код: " + IntToStr(RESTRspMIA->StatusCode) + "\nОтвет: " + RESTRspMIA->Content);
		}
	}
	catch (const Exception &e)
	{
		ShowUserMessage(lsFCB, llError, "Регистрация оборудывания", "Произошла ошибка при выполнении запроса: " + e.Message);
	}
}

void __fastcall TTMIAKiwi::confirmActivationWithOTP()
{
	String otpCode;

	if (InputQuery("Подтверждение устройства", "Введите код из СМС:", otpCode))
	{
		if (otpCode.Trim().IsEmpty())
		{
			ShowUserMessage(lsUNA, llWarning,"Регистрация оборудывания", "Код не может быть пустым. Попробуйте снова.");
			return;
		}

		RESTRqstMIA->Resource = "v1/terminal-activation/confirm";
		RESTRqstMIA->Method = TRESTRequestMethod::rmPOST;
		RESTRqstMIA->Params->Clear();
		RESTRqstMIA->ClearBody();

		String jsonStringBody = "{\"terminalActivationId\": \"" + this->FCurrentTerminalActivationId + "\", "
			"\"otpCode\": \"" + otpCode + "\"}";
		RESTRqstMIA->Body->Add(jsonStringBody, TRESTContentType::ctAPPLICATION_JSON);

		try
		{
			RESTRqstMIA->Execute();
			if (RESTRspMIA->StatusCode == 200)
			{
				ShowUserMessage(lsFCB, llInfo,"Регистрация оборудывания", "Устройство успешно активировано и привязано к терминалу!");
			}
			else
			{
				ShowUserMessage(lsFCB, llError,"Регистрация оборудывания", "Ошибка подтверждения. Код: " + IntToStr(RESTRspMIA->StatusCode) + "\nОтвет: " + RESTRspMIA->Content);
			}
		}
		catch (const Exception &e)
		{
			ShowUserMessage(lsFCB, llError,"Регистрация оборудывания", "Произошла ошибка при отправке кода подтверждения: " + e.Message);
		}
	}
	else
	{
		ShowUserMessage(lsUNA, llInfo,"Регистрация оборудывания", "Операция активации отменена.");
	}
}

String __fastcall TTMIAKiwi::genGUID()
{
	String installationId = "";
	TGUID guid;
	if (CreateGUID(guid) == S_OK)
	{
		installationId = System::Sysutils::GUIDToString(guid);
		installationId = installationId.SubString(2, installationId.Length() - 2);
		return installationId;
	}
	else
	{
		ShowUserMessage(lsUNA, llError,"Регистрация оборудывания", "Не удалось сгенерировать уникальный идентификатор устройства.");
	}
	return "";
}

void __fastcall TTMIAKiwi::SetAuthHeader()
{
	RESTRqstMIA->Params->Clear();
	RESTClntMIA->BaseURL = ReadStrParam(rpmDBFirst, "MIAAPiURL", "http://localhost:5000/pos/api/");

	if (!RESTClntMIA->BaseURL.IsEmpty() && RESTClntMIA->BaseURL[RESTClntMIA->BaseURL.Length()] != '/')
	{
		RESTClntMIA->BaseURL = RESTClntMIA->BaseURL + "/";
	}

	RESTRqstMIA->Accept = "application/json";
	RESTRqstMIA->AcceptCharset = "utf-8";
	RESTRqstMIA->Params->AddHeader("X-DEVICE", "UNA.MD APP");
	RESTRqstMIA->Params->AddHeader("X-PLATFORM-TYPE", "web");
	RESTRqstMIA->Params->AddHeader("X-PLATFORM-VERSION", "1.1");
	RESTRqstMIA->Params->AddHeader("X-INSTALLATION-ID", FinstallationId);
	RESTRqstMIA->Params->AddHeader("X-POS-APP-VERSION", "1.0.1");

	Log("[FCB MIA API] SetAuthHeader | BaseURL: " + RESTClntMIA->BaseURL + " | InstallationID: " + FinstallationId);
}

void __fastcall TTMIAKiwi::SetAuthHeaders()
{
}

bool __fastcall TTMIAKiwi::Login(const String& AUsername, const String& APassword)
{
	String terminalActivationId = FCurrentTerminalActivationId;
	if (terminalActivationId.IsEmpty())
	{
		Log("[FCB MIA API] Login ERROR | terminalActivationId is EMPTY");
		ShowUserMessage(lsUNA, llError,"Аутентификация", "terminalActivationId пуст! Необходимо сначала активировать терминал.");
		return false;
	}

	RESTClntMIA->Authenticator = NULL;
	SetAuthHeader();
	RESTRqstMIA->Resource = "v1/seller/login";
	RESTRqstMIA->Method = TRESTRequestMethod::rmPOST;
	RESTRqstMIA->ClearBody();

	String jsonBody = "{\"terminalActivationId\": \"" + terminalActivationId + "\", "
		"\"username\": \"" + AUsername + "\", "
		"\"password\": \"" + APassword + "\"}";
	RESTRqstMIA->Body->Add(jsonBody, TRESTContentType::ctAPPLICATION_JSON);

	Log(System::Sysutils::Format("[FCB MIA API] Login REQUEST | URL: %s%s | Body: %s", ARRAYOFCONST((RESTClntMIA->BaseURL, RESTRqstMIA->Resource, jsonBody))));

	try
	{
		RESTRqstMIA->Execute();
		String responseContent = RESTRspMIA->Content;
		if (responseContent.IsEmpty())
		{
			responseContent = "<EMPTY RESPONSE>";
		}

		Log(System::Sysutils::Format("[FCB MIA API] Login RESPONSE | Status: %d | StatusText: %s | Content: %s", ARRAYOFCONST((RESTRspMIA->StatusCode, RESTRspMIA->StatusText, responseContent))));

		if (RESTRspMIA->StatusCode == 200)
		{
			TJSONObject* json = (TJSONObject*)TJSONObject::ParseJSONValue(RESTRspMIA->Content);
			if (json)
			{
				try
				{
					String result = json->GetValue("result")->Value();
					if (result == "success")
					{
						TJSONObject* tokens = (TJSONObject*)json->GetValue("authTokens");
						this->FAccessToken = tokens->GetValue("accessToken")->Value();
						this->FRefreshToken = tokens->GetValue("refreshToken")->Value();

						Log(System::Sysutils::Format("[FCB MIA API] Login SUCCESS | AccessToken received (last 6 chars): ......%s", ARRAYOFCONST((FAccessToken.SubString(FAccessToken.Length() - 5, 6)))));

						t2authentificator->AccessToken = this->FAccessToken;
						RESTClntMIA->Authenticator = t2authentificator;
						return true;
					}
					else
					{
						Log("[FCB MIA API] Login FAILED | Result from server: " + result);
					}
				}
				__finally
				{
					delete json;
				}
			}
			else
			{
				Log("[FCB MIA API] Login ERROR | Failed to parse JSON response");
			}
		}
		else
		{
			Log("[FCB MIA API] Login ERROR | Non-200 status code");
		}

		ShowUserMessage(lsFCB, llError,"Аутентификация", "Ошибка входа. Статус: " + IntToStr(RESTRspMIA->StatusCode) + "\n" + responseContent);
		return false;
	}
	catch (const Exception& e)
	{
		String responseContent = RESTRspMIA->Content;
		if (responseContent.IsEmpty())
		{
			responseContent = "<NO RESPONSE CONTENT>";
		}
		Log("[FCB MIA API] Login EXCEPTION | Message: " + e.Message + " | Status: " + IntToStr(RESTRspMIA->StatusCode) + " | Response: " + responseContent);
		ShowUserMessage(lsFCB, llError,"Аутентификация", "Исключение при входе: " + e.Message + "\nОтвет сервера: " + responseContent);
		return false;
	}
}

bool __fastcall TTMIAKiwi::OpenShift()
{
	RESTRqstMIA->ClearBody();
	SetAuthHeader();
	SetAuthHeaders();
	RESTRqstMIA->Resource = "v1/shift/open";
	RESTRqstMIA->Method = TRESTRequestMethod::rmPOST;

	Log("[FCB MIA API] OpenShift REQUEST | URL: " + RESTClntMIA->BaseURL + RESTRqstMIA->Resource);

	try
	{
		RESTRqstMIA->Execute();
		Log("[FCB MIA API] OpenShift RESPONSE | Status: " + IntToStr(RESTRspMIA->StatusCode) + " | Content: " + RESTRspMIA->Content);

		if (RESTRspMIA->StatusCode == 200)
		{
			TJSONObject* json = (TJSONObject*)TJSONObject::ParseJSONValue(RESTRspMIA->Content);
			String result = json->GetValue("result")->Value();

			if (result == "success" || result == "already_opened")
			{
				this->FShiftId = json->GetValue("shiftId")->Value();
				Log("[FCB MIA API] OpenShift SUCCESS | ShiftId: " + FShiftId + " | Result: " + result);
				delete json;
				return true;
			}
			delete json;
		}

		Log("[FCB MIA API] OpenShift ERROR | Status: " + IntToStr(RESTRspMIA->StatusCode));
		ShowUserMessage(lsFCB, llError,"Аутентификация", "Ошибка открытия смены: " + RESTRspMIA->Content);
		return false;
	}
	catch (const Exception& e)
	{
		Log("[FCB MIA API] OpenShift EXCEPTION | " + e.Message);
		ShowUserMessage(lsFCB, llError,"Аутентификация", "Исключение при открытии смены: " + e.Message);
		return false;
	}
}

bool __fastcall TTMIAKiwi::CreateHybridQr()
{
	Log("[FCB MIA API] CreateHybridQr | Checking for an existing active QR before creation...");
	if (FindActiveHybridQr())
	{
		Log(System::Sysutils::Format("[FCB MIA API] CreateHybridQr | Found active QR ID: %s. Attempting to cancel it first.", ARRAYOFCONST((this->FQrId))));
		if (CancelQrById(this->FQrId))
		{
			Log("[FCB MIA API] CreateHybridQr | Successfully canceled the old QR code.");
		}
		else
		{
			Log("[FCB MIA API] CreateHybridQr WARNING | Failed to cancel the old QR code. Continuing with creation attempt...");
		}
	}
	else
	{
		Log("[FCB MIA API] CreateHybridQr | No active hybrid QR found. Proceeding to create a new one.");
	}

	RESTRqstMIA->Resource = "v2/qr";
	RESTRqstMIA->Method = TRESTRequestMethod::rmPOST;
	SetAuthHeader();
	SetAuthHeaders();
	RESTRqstMIA->ClearBody();

	String jsonBody = "{\"qrType\": \"hybrid\", \"amountType\": \"fixed\"}";
	RESTRqstMIA->Body->Add(jsonBody, TRESTContentType::ctAPPLICATION_JSON);

	Log("[FCB MIA API] CreateHybridQr REQUEST | URL: " + RESTClntMIA->BaseURL + RESTRqstMIA->Resource + " | Body: " + jsonBody);

	try
	{
		RESTRqstMIA->Execute();
		Log("[FCB MIA API] CreateHybridQr RESPONSE | Status: " + IntToStr(RESTRspMIA->StatusCode) + " | Content: " + RESTRspMIA->Content);

		if (RESTRspMIA->StatusCode == 200)
		{
			TJSONObject* json = NULL;
			try
			{
				json = (TJSONObject*)TJSONObject::ParseJSONValue(RESTRspMIA->Content);
				if (!json)
				{
					Log("[FCB MIA API] CreateHybridQr ERROR | Failed to parse JSON");
					throw Exception("Не удалось разобрать JSON ответ от сервера.");
				}

				String result = json->GetValue("result")->Value();
				if (result == "success")
				{
					TJSONObject* qrInfo = (TJSONObject*)json->GetValue("qrInfo");
					if (qrInfo)
					{
						this->FQrId = qrInfo->GetValue("qrId")->Value();
						TJSONObject* ext = (TJSONObject*)qrInfo->GetValue("extension");
						if (ext)
						{
							this->FExtensionId = ext->GetValue("extensionId")->Value();
						}
						WriteStrParam("FQrID", FQrId);
						Log("[FCB MIA API] CreateHybridQr SUCCESS | QrId: " + FQrId + " | ExtensionId: " + FExtensionId);

						String base64ImageString = qrInfo->GetValue("qrImage")->Value();
						DecodeBase64ToImage(base64ImageString, FCBQrImage);
						pnButSaveQRFCB->Enabled = true;
						return true;
					}
					else
					{
						Log("[FCB MIA API] CreateHybridQr ERROR | qrInfo field missing");
						throw Exception("В успешном ответе отсутствует обязательное поле 'qrInfo'.");
					}
				}
				else if (result == "exist_active_qr")
				{
					Log("[FCB MIA API] CreateHybridQr INFO | QR already exists, searching for active");
					ShowUserMessage(lsFCB, llInfo,"Оборудывание", "QR уже существует, ищем активный...");
					return FindActiveHybridQr();
				}
				else
				{
					Log("[FCB MIA API] CreateHybridQr ERROR | Unexpected result: " + result);
					ShowUserMessage(lsFCB, llError,"Оборудывание", "Сервер вернул статус, отличный от 'success': " + result);
					return false;
				}
			}
			__finally
			{
				delete json;
			}
		}

		Log("[FCB MIA API] CreateHybridQr ERROR | Status: " + IntToStr(RESTRspMIA->StatusCode));
		ShowUserMessage(lsFCB, llError,"Оборудывание", "Ошибка создания QR. Код ответа: " + IntToStr(RESTRspMIA->StatusCode));
		return false;
	}
	catch (const Exception& e)
	{
		Log("[FCB MIA API] CreateHybridQr EXCEPTION | " + e.Message);
		ShowUserMessage(lsFCB, llError,"Оборудывание", "Исключение при создании QR: " + e.Message);
		return false;
	}
	return false;
}

bool __fastcall TTMIAKiwi::FindActiveHybridQr()
{
	RESTRqstMIA->Resource = "v2/qr/active";
	RESTRqstMIA->Method = TRESTRequestMethod::rmGET;
	SetAuthHeader();
	SetAuthHeaders();
	RESTRqstMIA->Params->AddItem("type", "hybrid", TRESTRequestParameterKind::pkGETorPOST);
	RESTRqstMIA->ClearBody();

	try
	{
		RESTRqstMIA->Execute();
		if (RESTRspMIA->StatusCode == 200)
		{
			TJSONObject* json = (TJSONObject*)TJSONObject::ParseJSONValue(RESTRspMIA->Content);
			if (json->GetValue("result")->Value() == "success")
			{
				TJSONArray* codes = (TJSONArray*)json->GetValue("codes");
				if (codes->Count > 0)
				{
					TJSONObject* qrInfo = (TJSONObject*)codes->Items[0];
					this->FQrId = qrInfo->GetValue("qrId")->Value();
					delete json;
					return true;
				}
			}
			delete json;
		}
		ShowUserMessage(lsFCB, llInfo,"Оборудывание", "Активный QR не найден: " + RESTRspMIA->Content);
		return false;
	}
	catch (const Exception& e)
	{
		ShowUserMessage(lsFCB, llWarning,"Оборудывание", "Исключение при поиске QR: " + e.Message);
		return false;
	}
}

bool __fastcall TTMIAKiwi::CreatePaymentRequest(double AAmount)
{
	LoadMIAQRImage();
	pnBtnRepeat->Enabled = false;
	pnSumToPay->Caption = FloatToStr(AAmount) + " MDL";
	RESTRqstMIA->Resource = "v2/qr/" + this->FQrId + "/extension";
	RESTRqstMIA->Method = TRESTRequestMethod::rmPOST;
	SetAuthHeader();
	RESTRqstMIA->ClearBody();

	String amountStr = StringReplace(FloatToStrF(AAmount, ffFixed, 7, 2), ",", ".", TReplaceFlags() << rfReplaceAll);
	String jsonBody = "{\"amount\": " + amountStr + "}";
	RESTRqstMIA->Body->Add(jsonBody, TRESTContentType::ctAPPLICATION_JSON);

	Log(System::Sysutils::Format("[FCB MIA API] CreatePaymentRequest REQUEST | URL: %s%s | Amount: %s | Body: %s", ARRAYOFCONST((RESTClntMIA->BaseURL, RESTRqstMIA->Resource, amountStr, jsonBody))));

	try
	{
		RESTRqstMIA->Execute();
		Log(System::Sysutils::Format("[FCB MIA API] CreatePaymentRequest RESPONSE | Status: %d | Content: %s", ARRAYOFCONST(((int)RESTRspMIA->StatusCode, RESTRspMIA->Content))));

		if (RESTRspMIA->StatusCode == 200)
		{
			TJSONObject* json = (TJSONObject*)TJSONObject::ParseJSONValue(RESTRspMIA->Content);
			if (json)
			{
				try
				{
					TJSONObject* extInfo = (TJSONObject*)json->GetValue("extensionInfo");
					if (extInfo)
					{
						this->FExtensionId = extInfo->GetValue("extensionId")->Value();
						int expireInSeconds = 0;
						TJSONValue* expireValue = extInfo->GetValue("qrExpireIn");

						if (expireValue != NULL && dynamic_cast<TJSONNull*>(expireValue) == NULL)
						{
							TJSONNumber* numValue = dynamic_cast<TJSONNumber*>(expireValue);
							if (numValue != NULL)
							{
								expireInSeconds = numValue->AsInt;
							}
							else
							{
								try
								{
									expireInSeconds = StrToInt(expireValue->Value());
								}
								catch (const Exception &e)
								{
									expireInSeconds = 0;
								}
							}
						}

						if (expireInSeconds > 0)
						{
							valueTimer = expireInSeconds;
							pbTimeMIA->Max = valueTimer;
							tmrQr->Enabled = true;
						}
						Log(System::Sysutils::Format("[FCB MIA API] CreatePaymentRequest SUCCESS | ExtensionId: %s | ExpireIn: %ds", ARRAYOFCONST((FExtensionId, expireInSeconds))));
					}
					else
					{
						Timer1->Enabled = true;
						Log("[FCB MIA API] CreatePaymentRequest ERROR | 'extensionInfo' object not found in JSON response.");
					}
				}
				__finally
				{
					delete json;
				}
			}
			return true;
		}

		Log(System::Sysutils::Format("[FCB MIA API] CreatePaymentRequest ERROR | Status: %d", ARRAYOFCONST(((int)RESTRspMIA->StatusCode))));
		Timer1->Enabled = true;
		return false;
	}
	catch (const Exception& e)
	{
		Log("[FCB MIA API] CreatePaymentRequest EXCEPTION | " + e.Message);
		Timer1->Enabled = true;
		return false;
	}
}

String __fastcall TTMIAKiwi::CheckPaymentStatus()
{
	RESTRqstMIA->Resource = "v2/qr/extension/" + this->FExtensionId + "/check";
	RESTRqstMIA->Method = TRESTRequestMethod::rmGET;
	SetAuthHeader();
	RESTRqstMIA->ClearBody();

	Log(System::Sysutils::Format("[FCB MIA API] CheckPaymentStatus REQUEST | URL: %s%s | ExtensionId: %s", ARRAYOFCONST((RESTClntMIA->BaseURL, RESTRqstMIA->Resource, FExtensionId))));

	try
	{
		RESTRqstMIA->Execute();
		if (RESTRspMIA->StatusCode == 200)
		{
			TJSONObject* json = (TJSONObject*)TJSONObject::ParseJSONValue(RESTRspMIA->Content);
			if (json)
			{
				try
				{
					String status = json->GetValue("state")->Value();
					Log(System::Sysutils::Format("[FCB MIA API] CheckPaymentStatus RESPONSE | Status: %d | State: %s", ARRAYOFCONST(((int)RESTRspMIA->StatusCode, status))));
					return status;
				}
				__finally
				{
					delete json;
				}
			}
		}

		Log(System::Sysutils::Format("[FCB MIA API] CheckPaymentStatus ERROR | Status: %d | Content: %s", ARRAYOFCONST(((int)RESTRspMIA->StatusCode, RESTRspMIA->Content))));
		return "error";
	}
	catch (const Exception& e)
	{
		Log("[FCB MIA API] CheckPaymentStatus EXCEPTION | " + e.Message);
		return "exception";
	}
}

bool __fastcall TTMIAKiwi::SimulatePayment(double AAmount)
{
	ShowMessage("Имитация оплаты через тестовый сервер...");
	String integrationUrl = "https://mia-integration.redriverapps.net";
	String oldBaseUrl = RESTClntMIA->BaseURL;
	RESTClntMIA->BaseURL = integrationUrl;

	RESTRqstMIA->Resource = "/external-integration/api/v1/qr/signals";
	RESTRqstMIA->Method = TRESTRequestMethod::rmPOST;
	RESTRqstMIA->Params->Clear();
	RESTRqstMIA->Params->AddHeader("Content-Type", "application/json; charset=utf-8");
	RESTRqstMIA->Params->AddHeader("X-Request-ID", genGUID());
	RESTRqstMIA->Params->AddHeader("Receiver-Participant-Code", "participant-code-example");

	String amountStr = StringReplace(FloatToStrF(AAmount, ffFixed, 7, 2), ",", ".", TReplaceFlags() << rfReplaceAll);
	String dtTm = FormatDateTime("yyyy-mm-dd\"T\"hh:nn:ss", Now()) + "Z";
	String jsonBody = "[{\"signalCode\": \"Payment\", \"signalDtTm\": \"" + dtTm + "\", "
		"\"qrHeaderUUID\": \"" + this->FQrId + "\", "
		"\"payment\": {\"system\": \"IPS\", \"reference\": \"pacs.008|" + IntToStr(static_cast<__int64>(time(0))) + "\", "
		"\"amount\": {\"sum\": \"" + amountStr + "\", \"currency\": \"MDL\"}, "
		"\"dtTm\": \"" + dtTm + "\"}}]";
	RESTRqstMIA->Body->Add(jsonBody, TRESTContentType::ctAPPLICATION_JSON);

	bool success = false;
	try
	{
		RESTRqstMIA->Execute();
		if (RESTRspMIA->StatusCode >= 200 && RESTRspMIA->StatusCode < 300)
		{
			ShowMessage("Сигнал об оплате успешно отправлен.");
			success = true;
		}
		else
		{
			ShowMessage("Ошибка отправки сигнала оплаты: " + RESTRspMIA->Content);
		}
	}
	catch (const Exception& e)
	{
		ShowMessage("Исключение при имитации оплаты: " + e.Message);
	}

	RESTClntMIA->BaseURL = oldBaseUrl;
	return success;
}

void __fastcall TTMIAKiwi::CloseShift()
{
	RESTRqstMIA->ClearBody();
	SetAuthHeader();
	SetAuthHeaders();
	RESTRqstMIA->Resource = "v1/shift/close";
	RESTRqstMIA->Method = TRESTRequestMethod::rmPOST;
	RESTRqstMIA->Params->AddHeader("Content-Type", "application/json; charset=utf-8");

	try
	{
		RESTRqstMIA->Execute();
	}
	catch (const Exception& e)
	{
		ShowUserMessage(lsFCB, llWarning,"Ауьегьтфткацтя", "Исключение при закрытии смены: " + e.Message);
	}
}

void __fastcall TTMIAKiwi::Logout()
{
	CloseShift();
	RESTRqstMIA->Resource = "v1/seller/logout";
	RESTRqstMIA->Method = TRESTRequestMethod::rmPOST;
	SetAuthHeader();

	if (this->FRefreshToken.IsEmpty())
	{
		Log("Пустой refreshToken при logOut");
	}
	String jsonBody = "{\"refreshToken\": \"" + this->FRefreshToken + "\"}";
	RESTRqstMIA->Body->Add(jsonBody, TRESTContentType::ctAPPLICATION_JSON);
	try
	{
		RESTRqstMIA->Execute();
	}
	catch (const Exception& e)
	{
		ShowUserMessage(lsFCB, llWarning,"Аутентификация", "Исключение при выходе: " + e.Message);
	}

	this->FAccessToken = "";
	this->FRefreshToken = "";
	this->FShiftId = "";
	this->FQrId = "";
	RESTClntMIA->Authenticator = NULL;
	Log("[FCB MIA API] Logout | Authenticator cleared.");
}

void __fastcall TTMIAKiwi::pnBtnExitClick(TObject *Sender)
{
	this->Close();
}

void __fastcall TTMIAKiwi::pnBtnExitMouseDown(TObject *Sender, TMouseButton Button, TShiftState Shift, int X, int Y)
{
	pnBtnExit->BevelKind = Vcl::Controls::bkNone;
	pnBtnExit->BevelInner = Vcl::Controls::bvNone;
}

void __fastcall TTMIAKiwi::pnBtnExitMouseEnter(TObject *Sender)
{
	pnButText->Font->Color = clScrollBar;
}

void __fastcall TTMIAKiwi::pnBtnExitMouseLeave(TObject *Sender)
{
	pnButText->Font->Color = clWindowText;
}

void __fastcall TTMIAKiwi::pnBtnExitMouseUp(TObject *Sender, TMouseButton Button, TShiftState Shift, int X, int Y)
{
	pnBtnExit->BevelKind = Vcl::Controls::bkTile;
	pnBtnExit->BevelInner = Vcl::Controls::bvRaised;
}

void __fastcall TTMIAKiwi::SetOperationType(AnsiString NewValue)
{
	if (FMIAType == NewValue)
	{
		return;
	}
	FMIAType = NewValue;
}

void __fastcall TTMIAKiwi::pnButSaveQRFCBClick(TObject *Sender)
{
	FCBQrImage->Picture->SaveToFile("miaQR.png");
	ShowUserMessage(lsUNA, llInfo,"Оборудывание", "Poza a fost salvata ca: MIAQR.png in folder cu program");
}

void __fastcall TTMIAKiwi::pnButSaveQRFCBMouseDown(TObject *Sender, TMouseButton Button, TShiftState Shift, int X, int Y)
{
	pnButSaveQRFCB->BevelKind = Vcl::Controls::bkNone;
	pnButSaveQRFCB->BevelInner = Vcl::Controls::bvNone;
}

void __fastcall TTMIAKiwi::pnButSaveQRFCBMouseEnter(TObject *Sender)
{
	pnButSaveText->Font->Color = clScrollBar;
}

void __fastcall TTMIAKiwi::pnButSaveQRFCBMouseLeave(TObject *Sender)
{
	pnButSaveText->Font->Color = clWindowText;
}

void __fastcall TTMIAKiwi::pnButSaveQRFCBMouseUp(TObject *Sender, TMouseButton Button, TShiftState Shift, int X, int Y)
{
	pnButSaveQRFCB->BevelKind = Vcl::Controls::bkTile;
	pnButSaveQRFCB->BevelInner = Vcl::Controls::bvRaised;
}

void __fastcall TTMIAKiwi::savePassData(String instalationID, String terminalActivationId)
{
	String jSonToken = "{\"installationId\": \"" + instalationID + "\", \"terminalActivationId\": \"" + terminalActivationId + "\"}";
	try
	{
		DTM->qsMIATokenInsert->ParamByName("PosID")->AsString = DTM->PosID;
		DTM->qsMIATokenInsert->ParamByName("Token")->AsString = jSonToken;
		DTM->qsMIATokenInsert->ExecSQL();
	}
	catch (Exception &e)
	{
		ShowUserMessage(lsUNA, llError,"Работа с базой", "Произошла ошибка при вызове процедуры:\n" + e.Message);
	}
}

void __fastcall TTMIAKiwi::verifyAndSetAPIData()
{
	DTM->qsMIATokenSelect->ParamByName("PosID")->AsInteger = DTM->PosID;
	DTM->qsMIATokenSelect->Open();

	if (MIAType == "Qiwi")
	{
		if (DTM->qsMIATokenSelect->RecordCount > 0)
		{
			FData->Token = DTM->qsMIATokenSelect->FieldByName("Token")->AsString;
			FAuthToken = "Bearer " + FData->Token;
			mmoInfoMIA->Lines->Add("Токен загружен из БД: " + FData->Token);
		}
		else
		{
			Authenticate();
		}
	}
	else if (MIAType == "FCB")
	{
		if (DTM->qsMIATokenSelect->RecordCount > 0)
		{
			TJSONObject* jsonToken = (TJSONObject*)TJSONObject::ParseJSONValue(DTM->qsMIATokenSelect->FieldByName("Token")->AsString);
			if (jsonToken)
			{
				try
				{
					FinstallationId = jsonToken->GetValue("installationId")->Value();
					FCurrentTerminalActivationId = jsonToken->GetValue("terminalActivationId")->Value();

					if (!FCurrentTerminalActivationId.IsEmpty())
					{
						RESTClntMIA->BaseURL = ReadStrParam(rpmDBFirst, "MIAAPiURL", "http://localhost:5000/pos/api/");
						if (Login(DTM->kiwiLogin, DTM->kiwiPassw))
						{
							if (OpenShift())
							{
								Log("[FCB MIA API] Attempting to find an active hybrid QR...");
								if (!FindActiveHybridQr())
								{
									Log("[FCB MIA API] No active QR found. Creating a new hybrid QR...");
									if (!CreateHybridQr())
									{
										ShowUserMessage(lsFCB, llError,"Оборудывание", "Критическая ошибка: Не удалось найти или создать активный QR-код. Дальнейшие операции по оплате невозможны.");
										return;
									}
									else
									{
										Log(System::Sysutils::Format("[FCB MIA API] Successfully created new QR with ID: %s", ARRAYOFCONST((this->FQrId))));
									}
								}
								else
								{
									Log(System::Sysutils::Format("[FCB MIA API] Found active QR with ID: %s", ARRAYOFCONST((this->FQrId))));
								}
							}
						}
					}
				}
				__finally
				{
					delete jsonToken;
				}
			}
		}
	}
}

void __fastcall TTMIAKiwi::RESTRqstMIAAfterExecute(TCustomRESTRequest *Sender)
{
	try
	{
		String method;
		switch (RESTRqstMIA->Method)
		{
			case TRESTRequestMethod::rmGET: method = "GET"; break;
			case TRESTRequestMethod::rmPOST: method = "POST"; break;
			case TRESTRequestMethod::rmPUT: method = "PUT"; break;
			case TRESTRequestMethod::rmDELETE: method = "DELETE"; break;
			case TRESTRequestMethod::rmPATCH: method = "PATCH"; break;
			default: method = "UNKNOWN"; break;
		}

		String fullURL = RESTClntMIA->BaseURL + RESTRqstMIA->Resource;
		String headers = "";
		for (int i = 0; i < RESTRqstMIA->Params->Count; i++)
		{
			TRESTRequestParameter* p = RESTRqstMIA->Params->Items[i];
			if (p->Kind == TRESTRequestParameterKind::pkHTTPHEADER)
			{
				if (!headers.IsEmpty())
				{
					headers += "; ";
				}
				headers += p->Value;
			}
		}

		String body = "";
		for (int i = 0; i < RESTRqstMIA->Params->Count; i++)
		{
			TRESTRequestParameter* p = RESTRqstMIA->Params->Items[i];
			if (p->Kind == TRESTRequestParameterKind::pkREQUESTBODY)
			{
				body = p->Value;
				break;
			}
		}

		String statusCode = "N/A";
		String statusText = "";
		String responseBody = "";
		String responseHeaders = "";

		TCustomRESTResponse *response = RESTRqstMIA->Response;
		if (response != NULL)
		{
			statusCode = IntToStr(response->StatusCode);
			statusText = response->StatusText;
			responseBody = response->Content;

			if (response->Headers != NULL && response->Headers->Count > 0)
			{
				for (int i = 0; i < response->Headers->Count; i++)
				{
					if (!responseHeaders.IsEmpty())
					{
						responseHeaders += "; ";
					}
					responseHeaders += response->Headers->Strings[i];
				}
			}

			if (responseBody.Length() > 500)
			{
				responseBody = responseBody.SubString(1, 500) + "...";
			}
		}

		String logLine = "METHOD: " + method + " | URL: " + fullURL + " | HEADERS: " + headers + " | BODY: " + body + " | STATUS: " + statusCode + " " + statusText + " | RESPONSE_HEADERS: " + responseHeaders + " | RESPONSE: " + responseBody;
		Log(logLine);
	}
	catch (Exception &e)
	{
		Log("API_ERROR | " + e.Message);
	}
}

void __fastcall TTMIAKiwi::pnBtnDeviceSetClick(TObject *Sender)
{
	regNewPC();
}

void __fastcall TTMIAKiwi::pnBtnDeviceSetMouseEnter(TObject *Sender)
{
	pnTextDeviceSet->Font->Color = clScrollBar;
}

void __fastcall TTMIAKiwi::pnBtnDeviceSetMouseLeave(TObject *Sender)
{
	pnTextDeviceSet->Font->Color = clWindowText;
}

void __fastcall TTMIAKiwi::pnBtnDeviceSetMouseUp(TObject *Sender, TMouseButton Button, TShiftState Shift, int X, int Y)
{
	pnBtnDeviceSet->BevelKind = Vcl::Controls::bkTile;
	pnBtnDeviceSet->BevelInner = Vcl::Controls::bvRaised;
}

void __fastcall TTMIAKiwi::pnBtnDeviceSetMouseDown(TObject *Sender, TMouseButton Button, TShiftState Shift, int X, int Y)
{
	pnBtnDeviceSet->BevelKind = Vcl::Controls::bkNone;
	pnBtnDeviceSet->BevelInner = Vcl::Controls::bvNone;
}

void __fastcall TTMIAKiwi::mmoInfoMIAChange(TObject *Sender)
{
	Memo1->Lines = mmoInfoMIA->Lines;
}

void __fastcall TTMIAKiwi::pnBtnRepeatClick(TObject *Sender)
{
	pnBtnRepeat->Enabled = false;
	CreatePaymentRequest(currentSumToPay);
}

void __fastcall TTMIAKiwi::pnBtnRepeatMouseDown(TObject *Sender, TMouseButton Button, TShiftState Shift, int X, int Y)
{
	pnBtnRepeat->BevelKind = Vcl::Controls::bkNone;
}

void __fastcall TTMIAKiwi::pnBtnRepeatMouseEnter(TObject *Sender)
{
	pnTxtRepeat->Font->Color = clScrollBar;
}

void __fastcall TTMIAKiwi::pnBtnRepeatMouseLeave(TObject *Sender)
{
	pnTxtRepeat->Font->Color = clWindowText;
}

void __fastcall TTMIAKiwi::pnBtnRepeatMouseUp(TObject *Sender, TMouseButton Button, TShiftState Shift, int X, int Y)
{
	pnBtnRepeat->BevelKind = Vcl::Controls::bkFlat;
}

bool __fastcall TTMIAKiwi::CancelPaymentFCB()
{
	if (FExtensionId.IsEmpty())
	{
		Log("[FCB MIA API] CancelPaymentFCB ERROR | ExtensionId is EMPTY - nothing to cancel");
		ShowUserMessage(lsFCB, llWarning,"Оплата", "Нет активного платежа для отмены.");
		return false;
	}

	RESTRqstMIA->Resource = "v2/qr/extension/" + FExtensionId;
	RESTRqstMIA->Method = TRESTRequestMethod::rmDELETE;
	SetAuthHeader();
	SetAuthHeaders();
	RESTRqstMIA->ClearBody();

	Log("[FCB MIA API] CancelPaymentFCB REQUEST | URL: " + RESTClntMIA->BaseURL + RESTRqstMIA->Resource + " | ExtensionId: " + FExtensionId);

	try
	{
		RESTRqstMIA->Execute();
		Log("[FCB MIA API] CancelPaymentFCB RESPONSE | Status: " + IntToStr(RESTRspMIA->StatusCode) + " | Content: " + RESTRspMIA->Content);

		if (RESTRspMIA->StatusCode == 200)
		{
			TJSONObject* json = (TJSONObject*)TJSONObject::ParseJSONValue(RESTRspMIA->Content);
			if (json)
			{
				try
				{
					String result = json->GetValue("result")->Value();
					if (result == "success")
					{
						Log("[FCB MIA API] CancelPaymentFCB SUCCESS | Payment canceled successfully");
						mmoInfoMIA->Lines->Add("Платеж успешно отменен.");
						pnTextInfoPay->Caption = "Платеж отменен";
						tmrQr->Enabled = false;
						ModalResult = mrCancel;
						FExtensionId = "";
						return true;
					}
					else
					{
						Log("[FCB MIA API] CancelPaymentFCB WARNING | Unexpected result: " + result);
						ShowUserMessage(lsFCB, llWarning,"Оплата", "Сервер вернул неожиданный результат: " + result);
					}
				}
				__finally
				{
					delete json;
				}
			}
			else
			{
				Log("[FCB MIA API] CancelPaymentFCB ERROR | Failed to parse JSON response");
			}
		}
		else if (RESTRspMIA->StatusCode == 404)
		{
			Log("[FCB MIA API] CancelPaymentFCB WARNING | Payment session not found (404)");
			ShowUserMessage(lsFCB, llWarning,"Оплата", "Платежная сессия не найдена или уже завершена.");
			tmrQr->Enabled = false;
			FExtensionId = "";
			return false;
		}
		else
		{
			Log("[FCB MIA API] CancelPaymentFCB ERROR | HTTP Status: " + IntToStr(RESTRspMIA->StatusCode));
			ShowUserMessage(lsFCB, llError,"Оплата", "Ошибка отмены платежа. Код: " + IntToStr(RESTRspMIA->StatusCode) + "\n" + RESTRspMIA->Content);
		}
	}
	catch (const Exception& e)
	{
		Log("[FCB MIA API] CancelPaymentFCB EXCEPTION | " + e.Message);
		ShowUserMessage(lsFCB, llError,"Оплата", "Исключение при отмене платежа: " + e.Message);
		return false;
	}
	return false;
}

void __fastcall TTMIAKiwi::pnAnulClick(TObject *Sender)
{
	if (tmrQr->Enabled)
	{
		CancelPaymentFCB();
	}
	else
	{
		ModalResult = mrCancel;
	}
}

void __fastcall TTMIAKiwi::pnAnulMouseDown(TObject *Sender, TMouseButton Button, TShiftState Shift, int X, int Y)
{
	pnAnul->BevelKind = Vcl::Controls::bkNone;
}

void __fastcall TTMIAKiwi::pnAnulMouseEnter(TObject *Sender)
{
	pnTxtAnul->Font->Color = clScrollBar;
}

void __fastcall TTMIAKiwi::pnAnulMouseLeave(TObject *Sender)
{
	pnTxtAnul->Font->Color = clWindowText;
}

void __fastcall TTMIAKiwi::pnAnulMouseUp(TObject *Sender, TMouseButton Button, TShiftState Shift, int X, int Y)
{
	pnAnul->BevelKind = Vcl::Controls::bkFlat;
}

void __fastcall TTMIAKiwi::Timer1Timer(TObject *Sender)
{
	Timer1->Enabled = false;
	String ErrorMsg = "Ошибка создания счёта. ";
	if (!RESTRspMIA->Content.IsEmpty())
	{
		ErrorMsg += "Ответ сервера: " + RESTRspMIA->Content;
	}
	else
	{
		ErrorMsg += "Возможно, проблема с сетью или конфигурацией. Проверьте логи.";
	}
	ShowUserMessage(lsFCB, llError,"Оплата", ErrorMsg);
	ModalResult = mrCancel;
}

bool __fastcall TTMIAKiwi::CancelQrById(const String& qrId)
{
	if (qrId.IsEmpty())
	{
		Log("[FCB MIA API] CancelQrById ERROR | qrId is empty. Nothing to cancel.");
		return false;
	}

	RESTRqstMIA->Resource = "v2/qr/" + qrId;
	RESTRqstMIA->Method = TRESTRequestMethod::rmDELETE;
	SetAuthHeader();
	RESTRqstMIA->ClearBody();

	Log(System::Sysutils::Format("[FCB MIA API] CancelQrById REQUEST | URL: %s%s", ARRAYOFCONST((RESTClntMIA->BaseURL, RESTRqstMIA->Resource))));

	try
	{
		RESTRqstMIA->Execute();
		Log(System::Sysutils::Format("[FCB MIA API] CancelQrById RESPONSE | Status: %d | Content: %s", ARRAYOFCONST((RESTRspMIA->StatusCode, RESTRspMIA->Content))));

		if (RESTRspMIA->StatusCode == 200)
		{
			TJSONObject* json = (TJSONObject*)TJSONObject::ParseJSONValue(RESTRspMIA->Content);
			if (json)
			{
				try
				{
					if (json->GetValue("result")->Value() == "success")
					{
						Log("[FCB MIA API] CancelQrById SUCCESS | QR was canceled successfully.");
						return true;
					}
				}
				__finally
				{
					delete json;
				}
			}
		}

		Log("[FCB MIA API] CancelQrById FAILED.");
		return false;
	}
	catch (const Exception& e)
	{
		Log("[FCB MIA API] CancelQrById EXCEPTION | " + e.Message);
		return false;
	}
}

void __fastcall TTMIAKiwi::ShowUserMessage(TLogSource source, TLogLevel level, const String& function, const String& message)
{
	String sourceStr;

	switch (source)
	{
		case lsUNA: sourceStr = "UNA"; break;
		case lsFCB: sourceStr = "FCB MIA"; break;
		case lsQiwi: sourceStr = "QIWI"; break;
		case lsDatabase: sourceStr = "DATABASE"; break;
		case lsSystem: sourceStr = "SYSTEM"; break;
		case lsUI: sourceStr = "UI"; break;
		default: sourceStr = "UNKNOWN";
	}

	String userMessage = System::Sysutils::Format("[%s] %s\n\n%s", ARRAYOFCONST((sourceStr, function, message)));

	switch (level)
	{
		case llInfo:
		case llDebug:
			MessageDlg(userMessage, mtInformation, TMsgDlgButtons() << mbOK, 0);
			break;
		case llWarning:
			MessageDlg(userMessage, mtWarning, TMsgDlgButtons() << mbOK, 0);
			break;
		case llError:
			MessageDlg(userMessage, mtError, TMsgDlgButtons() << mbOK, 0);
			break;
		case llSuccess:
			Application->MessageBox(userMessage.w_str(), L"Успешно", MB_OK | MB_ICONINFORMATION);
			break;
		default:
			ShowMessage(userMessage);
	}
}
