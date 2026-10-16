# The one component of the e15 sidecar: sidecar.c over the uart and ledc drivers.
idf_component_register(SRCS "sidecar.c"
                       INCLUDE_DIRS ""
                       REQUIRES driver)
