#include "services/credential_store.h"

#include <QSettings>

#ifdef Q_OS_ANDROID
#include <QJniObject>
#include <QtCore/qcoreapplication_platform.h>
#endif

#ifdef Q_OS_WIN
#include <windows.h>
#include <wincred.h>
#endif

namespace amt {

CredentialStore::CredentialStore(QObject *parent)
    : QObject(parent)
{
}

bool CredentialStore::saveApiKey(ProviderKind provider, const QString &apiKey, QString *errorMessage, bool allowDelete) const
{
#ifdef AMT_TESTING
    QSettings settings;
    const QString key = QString("credentials/%1").arg(providerCredentialTarget(provider));
    if (apiKey.trimmed().isEmpty()) {
        if (allowDelete) {
            settings.remove(key);
        }
    } else {
        settings.setValue(key, apiKey);
    }
    Q_UNUSED(errorMessage);
    return true;
#elif defined(Q_OS_ANDROID)
    const QJniObject context = QNativeInterface::QAndroidApplication::context();
    const QJniObject target = QJniObject::fromString(providerCredentialTarget(provider));
    const QJniObject value = QJniObject::fromString(apiKey);
    const jboolean ok = QJniObject::callStaticMethod<jboolean>(
        "com/aimeetingtable/mobile/SecureCredentialStore",
        "saveApiKey",
        "(Landroid/content/Context;Ljava/lang/String;Ljava/lang/String;Z)Z",
        context.object<jobject>(),
        target.object<jstring>(),
        value.object<jstring>(),
        static_cast<jboolean>(allowDelete));
    if (!ok && errorMessage) {
        *errorMessage = "Android Keystore credential save failed.";
    }
    return ok;
#elif defined(Q_OS_WIN)
    const QString target = providerCredentialTarget(provider);
    if (apiKey.trimmed().isEmpty()) {
        if (!allowDelete) {
            return true;
        }
        if (!CredDeleteW(reinterpret_cast<LPCWSTR>(target.utf16()), CRED_TYPE_GENERIC, 0)) {
            const DWORD error = GetLastError();
            if (error != ERROR_NOT_FOUND) {
                if (errorMessage) {
                    *errorMessage = QString("Credential delete failed with Win32 error %1").arg(error);
                }
                return false;
            }
        }
        return true;
    }

    QByteArray utf16Data(reinterpret_cast<const char *>(apiKey.utf16()), apiKey.size() * static_cast<int>(sizeof(char16_t)));

    CREDENTIALW credential = {};
    credential.Type = CRED_TYPE_GENERIC;
    std::wstring targetW = target.toStdWString();
    credential.TargetName = const_cast<LPWSTR>(targetW.c_str());
    credential.Persist = CRED_PERSIST_LOCAL_MACHINE;
    credential.CredentialBlobSize = static_cast<DWORD>(utf16Data.size());
    credential.CredentialBlob = reinterpret_cast<LPBYTE>(utf16Data.data());
    std::wstring userW = L"AI Meeting Table";
    credential.UserName = const_cast<LPWSTR>(userW.c_str());

    if (!CredWriteW(&credential, 0)) {
        if (errorMessage) {
            *errorMessage = QString("Credential save failed with Win32 error %1").arg(GetLastError());
        }
        return false;
    }
    return true;
#else
    const QString target = providerCredentialTarget(provider);
    QSettings settings;
    if (apiKey.trimmed().isEmpty()) {
        if (allowDelete) {
            settings.remove(QString("credentials/%1").arg(target));
        }
        return true;
    }
    settings.setValue(QString("credentials/%1").arg(target), apiKey);
    Q_UNUSED(errorMessage);
    return true;
#endif
}

QString CredentialStore::loadApiKey(ProviderKind provider, QString *errorMessage) const
{
#ifdef AMT_TESTING
    Q_UNUSED(errorMessage);
    return QSettings().value(QString("credentials/%1").arg(providerCredentialTarget(provider))).toString();
#elif defined(Q_OS_ANDROID)
    const QJniObject context = QNativeInterface::QAndroidApplication::context();
    const QJniObject target = QJniObject::fromString(providerCredentialTarget(provider));
    const QJniObject value = QJniObject::callStaticObjectMethod(
        "com/aimeetingtable/mobile/SecureCredentialStore",
        "loadApiKey",
        "(Landroid/content/Context;Ljava/lang/String;)Ljava/lang/String;",
        context.object<jobject>(),
        target.object<jstring>());
    Q_UNUSED(errorMessage);
    return value.toString();
#elif defined(Q_OS_WIN)
    const QString target = providerCredentialTarget(provider);
    PCREDENTIALW rawCredential = nullptr;
    if (!CredReadW(reinterpret_cast<LPCWSTR>(target.utf16()), CRED_TYPE_GENERIC, 0, &rawCredential)) {
        if (errorMessage) {
            *errorMessage = QString("Credential read failed with Win32 error %1").arg(GetLastError());
        }
        return {};
    }

    const QString apiKey = QString::fromUtf16(reinterpret_cast<const char16_t *>(rawCredential->CredentialBlob),
                                              rawCredential->CredentialBlobSize / static_cast<DWORD>(sizeof(char16_t)));
    CredFree(rawCredential);
    return apiKey;
#else
    Q_UNUSED(errorMessage);
    return QSettings().value(QString("credentials/%1").arg(providerCredentialTarget(provider))).toString();
#endif
}

}
