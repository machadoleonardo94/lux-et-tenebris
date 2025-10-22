#if !defined(PROJECT_GLOBAL_CONSTANTS)
#define PROJECT_GLOBAL_CONSTANTS

//* Multiplier to convert microseconds to seconds.
#define one_second_in_microseconds 1000000ULL

//* Server Certificate for OTA remote updates on secure connections.
static const char *server_certificate =
    "-----BEGIN CERTIFICATE-----r\n"
    "ADDYOURSERVERCERTIFICATESHERE\r\n"
    "-----END CERTIFICATE-----\r\n";

#endif // PROJECT_GLOBAL_CONSTANTS
