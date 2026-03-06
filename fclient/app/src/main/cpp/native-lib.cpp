#include <jni.h>
#include <string>
#include <thread>
#include <cstring>
#include <android/log.h>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/android_sink.h>
#include <mbedtls/entropy.h>
#include <mbedtls/ctr_drbg.h>
#include <mbedtls/des.h>
#include <iomanip>
#include <sstream>

#define LOG_INFO(...) __android_log_print(ANDROID_LOG_INFO, "fclient_ndk", __VA_ARGS__)

#define SLOG_INFO(...) android_logger->info(__VA_ARGS__)
auto android_logger = spdlog::android_logger_mt("android", "fclient_ndk");

mbedtls_entropy_context entropy;
mbedtls_ctr_drbg_context ctr_drbg;
char *personalization = "fclient-sample-app";

// Вспомогательная функция для преобразования байтов в hex-строку
std::string bytesToHex(const uint8_t* bytes, size_t len) {
    std::stringstream ss;
    ss << std::hex << std::setfill('0');
    for (size_t i = 0; i < len; ++i) {
        ss << std::setw(2) << static_cast<int>(bytes[i]);
    }
    return ss.str();
}

// Вспомогательная функция для логирования jbyteArray
std::string jbyteArrayToHex(JNIEnv* env, jbyteArray array) {
    jsize len = env->GetArrayLength(array);
    jbyte* bytes = env->GetByteArrayElements(array, nullptr);
    std::string hex = bytesToHex(reinterpret_cast<uint8_t*>(bytes), len);
    env->ReleaseByteArrayElements(array, bytes, 0);
    return hex;
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_example_fclient_MainActivity_stringFromJNI(
        JNIEnv* env,
        jobject /* this */) {
    std::string hello = "Hello from C++";
    LOG_INFO("Hello from c++ %d", 2023);
    SLOG_INFO("Hello from spdlog {0}", 2023);
    return env->NewStringUTF(hello.c_str());
}

extern "C" JNIEXPORT jint JNICALL
Java_com_example_fclient_MainActivity_initRng(JNIEnv *env, jclass clazz) {
    mbedtls_entropy_init(&entropy);
    mbedtls_ctr_drbg_init(&ctr_drbg);

    int result = mbedtls_ctr_drbg_seed(&ctr_drbg, mbedtls_entropy_func, &entropy,
                                       (const unsigned char *)personalization,
                                       strlen(personalization));

    SLOG_INFO("RNG initialized with result: {0}", result);
    return result;
}

extern "C" JNIEXPORT jbyteArray JNICALL
Java_com_example_fclient_MainActivity_randomBytes(JNIEnv *env, jclass, jint no) {
    uint8_t *buf = new uint8_t[no];
    mbedtls_ctr_drbg_random(&ctr_drbg, buf, no);

    std::string hexStr = bytesToHex(buf, no);
    SLOG_INFO("Generated random bytes ({0} bytes): {1}", no, hexStr);

    jbyteArray rnd = env->NewByteArray(no);
    env->SetByteArrayRegion(rnd, 0, no, (jbyte *)buf);
    delete[] buf;
    return rnd;
}

extern "C" JNIEXPORT jbyteArray JNICALL
Java_com_example_fclient_MainActivity_encrypt(
        JNIEnv *env,
        jclass,
        jbyteArray key,
        jbyteArray data)
{
    jsize ksz = env->GetArrayLength(key);
    jsize dsz = env->GetArrayLength(data);

    // Логируем входные параметры
    std::string keyHex = jbyteArrayToHex(env, key);
    std::string dataHex = jbyteArrayToHex(env, data);
    SLOG_INFO("=== Encryption started ===");
    SLOG_INFO("Key length: {0}, Key: {1}", ksz, keyHex);
    SLOG_INFO("Data length: {0}, Data: {1}", dsz, dataHex);

    if ((ksz != 16) || (dsz <= 0)) {
        SLOG_INFO("Encryption failed: Invalid key size or data length");
        return env->NewByteArray(0);
    }

    mbedtls_des3_context ctx;
    mbedtls_des3_init(&ctx);

    jbyte *pkey = env->GetByteArrayElements(key, 0);

    // Паддинг PKCS#5
    int rst = dsz % 8;
    int sz = dsz + (rst == 0 ? 8 : 8 - rst);  // Если длина кратна 8, добавляем полный блок
    uint8_t *buf = new uint8_t[sz];

    // Копируем исходные данные
    jbyte *pdata = env->GetByteArrayElements(data, 0);
    std::copy(pdata, pdata + dsz, buf);

    // Добавляем паддинг
    uint8_t padValue = (rst == 0) ? 8 : (8 - rst);
    for (int i = dsz; i < sz; i++) {
        buf[i] = padValue;
    }

    SLOG_INFO("Padding applied: {0} bytes added, pad value: {1}", sz - dsz, padValue);
    SLOG_INFO("Data with padding: {0}", bytesToHex(buf, sz));

    // Шифрование
    mbedtls_des3_set2key_enc(&ctx, (uint8_t *)pkey);
    int cn = sz / 8;
    for (int i = 0; i < cn; i++) {
        mbedtls_des3_crypt_ecb(&ctx, buf + i * 8, buf + i * 8);
        SLOG_INFO("Encrypted block {0}: {1}", i, bytesToHex(buf + i * 8, 8));
    }

    jbyteArray dout = env->NewByteArray(sz);
    env->SetByteArrayRegion(dout, 0, sz, (jbyte *)buf);

    // Логируем результат
    std::string encryptedHex = bytesToHex(buf, sz);
    SLOG_INFO("Encryption complete. Encrypted data ({0} bytes): {1}", sz, encryptedHex);

    delete[] buf;
    env->ReleaseByteArrayElements(key, pkey, 0);
    env->ReleaseByteArrayElements(data, pdata, 0);

    return dout;
}

extern "C" JNIEXPORT jbyteArray JNICALL
Java_com_example_fclient_MainActivity_decrypt(
        JNIEnv *env,
        jclass,
        jbyteArray key,
        jbyteArray data)
{
    jsize ksz = env->GetArrayLength(key);
    jsize dsz = env->GetArrayLength(data);

    // Логируем входные параметры
    std::string keyHex = jbyteArrayToHex(env, key);
    std::string encryptedHex = jbyteArrayToHex(env, data);
    SLOG_INFO("=== Decryption started ===");
    SLOG_INFO("Key length: {0}, Key: {1}", ksz, keyHex);
    SLOG_INFO("Encrypted data length: {0}, Data: {1}", dsz, encryptedHex);

    if ((ksz != 16) || (dsz <= 0) || ((dsz % 8) != 0)) {
        SLOG_INFO("Decryption failed: Invalid key size or data length");
        return env->NewByteArray(0);
    }

    mbedtls_des3_context ctx;
    mbedtls_des3_init(&ctx);

    jbyte *pkey = env->GetByteArrayElements(key, 0);
    uint8_t *buf = new uint8_t[dsz];
    jbyte *pdata = env->GetByteArrayElements(data, 0);
    std::copy(pdata, pdata + dsz, buf);

    SLOG_INFO("Data to decrypt: {0}", bytesToHex(buf, dsz));

    // Дешифрование
    mbedtls_des3_set2key_dec(&ctx, (uint8_t *)pkey);
    int cn = dsz / 8;
    for (int i = 0; i < cn; i++) {
        mbedtls_des3_crypt_ecb(&ctx, buf + i * 8, buf + i * 8);
        SLOG_INFO("Decrypted block {0}: {1}", i, bytesToHex(buf + i * 8, 8));
    }

    // PKCS#5 удаление паддинга
    uint8_t padValue = buf[dsz - 1];
    if (padValue < 1 || padValue > 8) {
        SLOG_INFO("Warning: Invalid padding value: {0}", padValue);
        padValue = 0;
    }

    int sz = dsz - padValue;
    SLOG_INFO("Padding removed: {0} bytes (pad value: {1})", padValue, padValue);
    SLOG_INFO("Decrypted data without padding ({0} bytes): {1}", sz, bytesToHex(buf, sz));

    jbyteArray dout = env->NewByteArray(sz);
    env->SetByteArrayRegion(dout, 0, sz, (jbyte *)buf);

    SLOG_INFO("Decryption complete. Original data length: {0}", sz);

    delete[] buf;
    env->ReleaseByteArrayElements(key, pkey, 0);
    env->ReleaseByteArrayElements(data, pdata, 0);

    return dout;
}

JavaVM* gJvm = nullptr;

JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* pjvm, void* reserved) {
    gJvm = pjvm;
    SLOG_INFO("JNI_OnLoad called, JavaVM saved");
    return JNI_VERSION_1_6;
}

JNIEnv* getEnv(bool& detach) {
    JNIEnv* env = nullptr;
    int status = gJvm->GetEnv((void**)&env, JNI_VERSION_1_6);
    detach = false;

    if (status == JNI_EDETACHED) {
        status = gJvm->AttachCurrentThread(&env, NULL);
        if (status < 0) {
            return nullptr;
        }
        detach = true;
    }
    return env;
}

void releaseEnv(bool detach, JNIEnv* env) {
    if (detach && (gJvm != nullptr)) {
        gJvm->DetachCurrentThread();
    }
}

extern "C"
JNIEXPORT jboolean JNICALL
Java_com_example_fclient_MainActivity_transaction(JNIEnv *xenv, jobject xthiz, jbyteArray xtrd) {
    SLOG_INFO("=== Transaction started ===");

    // Логируем входные данные TRD
    std::string trdHex = jbyteArrayToHex(xenv, xtrd);
    SLOG_INFO("TRD data: {0}", trdHex);

    // Создаем глобальные ссылки
    jobject thiz = xenv->NewGlobalRef(xthiz);
    jbyteArray trd = (jbyteArray)xenv->NewGlobalRef(xtrd);

    std::thread t([thiz, trd] {
        bool detach = false;
        JNIEnv *env = getEnv(detach);

        if (!env) {
            SLOG_INFO("Failed to get JNIEnv in transaction thread");
            return;
        }

        jclass cls = env->GetObjectClass(thiz);
        jmethodID enterPinId = env->GetMethodID(
                cls, "enterPin", "(ILjava/lang/String;)Ljava/lang/String;");

        // Получаем данные TRD
        uint8_t* p = (uint8_t*)env->GetByteArrayElements(trd, 0);
        jsize sz = env->GetArrayLength(trd);

        // Проверяем формат TRD (9F02 06 + сумма)
        if ((sz != 9) || (p[0] != 0x9F) || (p[1] != 0x02) || (p[2] != 0x06)) {
            SLOG_INFO("Invalid TRD format: expected 9F0206...");
            env->ReleaseByteArrayElements(trd, (jbyte *)p, 0);

            // Вызываем transactionResult с false
            jmethodID resultId = env->GetMethodID(cls, "transactionResult", "(Z)V");
            env->CallVoidMethod(thiz, resultId, false);

            releaseEnv(detach, env);
            return;
        }

        // Преобразуем сумму в строку
        char buf[13];
        for (int i = 0; i < 6; i++) {
            uint8_t n = *(p + 3 + i);
            buf[i*2] = ((n & 0xF0) >> 4) + '0';
            buf[i*2 + 1] = (n & 0x0F) + '0';
        }
        buf[12] = 0;

        std::string amountStr(buf);
        SLOG_INFO("Transaction amount: {0} kopecks ({1} rubles)", amountStr,
                  amountStr.substr(0, amountStr.length() - 2) + "." + amountStr.substr(amountStr.length() - 2));

        jstring jamount = env->NewStringUTF(buf);
        int ptc = 3;
        bool success = false;

        // Запрашиваем PIN до 3 раз
        while (ptc > 0) {
            SLOG_INFO("Requesting PIN, attempts left: {0}", ptc);
            jstring pin = (jstring) env->CallObjectMethod(thiz, enterPinId, ptc, jamount);
            const char *utf = env->GetStringUTFChars(pin, nullptr);

            if (utf != nullptr) {
                SLOG_INFO("PIN entered: {0}", utf);
                bool pinCorrect = (strcmp(utf, "1234") == 0);

                if (pinCorrect) {
                    SLOG_INFO("PIN correct!");
                    success = true;
                } else {
                    SLOG_INFO("PIN incorrect");
                }

                env->ReleaseStringUTFChars(pin, utf);
            } else {
                SLOG_INFO("PIN entry cancelled or failed");
            }

            env->DeleteLocalRef(pin);

            if (success) {
                break;
            }
            ptc--;
        }

        env->ReleaseByteArrayElements(trd, (jbyte *)p, 0);
        env->DeleteLocalRef(jamount);

        SLOG_INFO("Transaction result: {0}", success ? "SUCCESS" : "FAILED");

        // Вызываем transactionResult с результатом
        jmethodID resultId = env->GetMethodID(cls, "transactionResult", "(Z)V");
        env->CallVoidMethod(thiz, resultId, success);

        // Очищаем глобальные ссылки
        env->DeleteGlobalRef(thiz);
        env->DeleteGlobalRef(trd);

        releaseEnv(detach, env);
        SLOG_INFO("Transaction thread finished");
    });

    t.detach();
    SLOG_INFO("Transaction thread detached and running");
    return true;
}