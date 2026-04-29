#ifndef ICE_CERTIFICATE_H_
#define ICE_CERTIFICATE_H_

#include <rtc_base/rtc_certificate.h>
#include <rtc_base/ssl_identity.h>

namespace ice {

enum KeyType { KT_RSA, KT_ECDSA, KT_LAST, KT_DEFAULT = KT_ECDSA };

enum ECCurve { EC_NIST_P256, /* EC_FANCY, */ EC_LAST };

class KeyParams {
public:
    explicit KeyParams(KeyType key_type = KT_DEFAULT);

    // Generate a a KeyParams for RSA with explicit parameters.
    static KeyParams RSA(int mod_size = rtc::kRsaDefaultModSize,
        int pub_exp = rtc::kRsaDefaultExponent);

    // Generate a a KeyParams for ECDSA specifying the curve.
    static KeyParams ECDSA(ECCurve curve = EC_NIST_P256);

    rtc::KeyParams params() const { return params_; }

private:
    rtc::KeyParams params_;
};

class Certificate {
public:
    static Certificate* Create(const KeyParams& key_params, uint64_t expires_time);
    ~Certificate() = default;

    rtc::scoped_refptr<rtc::RTCCertificate> cert() { return cert_; }

private:
    Certificate() = default;
    
private:
    rtc::scoped_refptr<rtc::RTCCertificate> cert_;
};

} // namespace ice

#endif // ICE_CERTIFICATE_H_