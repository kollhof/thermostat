idf_build_get_property(project_dir PROJECT_DIR)

idf_component_register(
  SRC_DIRS "."
  INCLUDE_DIRS "."
  EMBED_TXTFILES
    ${PROJECT_DIR}/secret/ota/ota_ca_cert.pem
    ${PROJECT_DIR}/secret/aws/aws-root-ca.pem
    ${PROJECT_DIR}/secret/aws/aws-certificate.pem.crt
    ${PROJECT_DIR}/secret/aws/aws-private.pem.key
)
