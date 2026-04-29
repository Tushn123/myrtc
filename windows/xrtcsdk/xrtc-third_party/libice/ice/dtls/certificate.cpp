#include "ice/dtls/certificate.h"

#include <rtc_base/rtc_certificate_generator.h>

namespace ice {

KeyParams::KeyParams(KeyType key_type) : params_((rtc::KeyType)key_type) {
}

// static
KeyParams KeyParams::RSA(int mod_size, int pub_exp) {
    KeyParams kt(KT_RSA);
    kt.params_ = rtc::KeyParams::RSA(mod_size, pub_exp);
    return kt;
}

// static
KeyParams KeyParams::ECDSA(ECCurve curve) {
    KeyParams kt(KT_ECDSA);
    kt.params_ = rtc::KeyParams::ECDSA((rtc::ECCurve)curve);
    return kt;
}

Certificate* Certificate::Create(const KeyParams& key_params, uint64_t expires_ms) {
    Certificate* cert = new Certificate();
    rtc::KeyParams params = key_params.params();
    cert->cert_ = rtc::RTCCertificateGenerator::GenerateCertificate(params, expires_ms);
    if (!cert->cert_) {
        delete cert;
        return nullptr;
    }
    return cert;
}

} // namespace ice