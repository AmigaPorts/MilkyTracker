string(TIMESTAMP NOW "%Y%m%d-%H%M%S")
file(WRITE ${HEADER_FILE} "#define NOW \"${NOW}\"")
