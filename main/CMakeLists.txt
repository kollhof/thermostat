idf_build_get_property(project_dir PROJECT_DIR)

# REQUIRES
# wifi_provisioning qrcode esp_hap_core esp_hap_platform nvs_flash
# app_update mqtt json esp_hap_apple_profiles esp_hap_extras button
# esp_https_ota slow-pwm temp-sensor

idf_component_register(
  SRC_DIRS "."
  INCLUDE_DIRS "."
  EMBED_TXTFILES
    ${PROJECT_DIR}/secret/ota/ota_ca_cert.pem
    ${PROJECT_DIR}/secret/aws/aws-root-ca.pem
    ${PROJECT_DIR}/secret/aws/aws-certificate.pem.crt
    ${PROJECT_DIR}/secret/aws/aws-private.pem.key
)
