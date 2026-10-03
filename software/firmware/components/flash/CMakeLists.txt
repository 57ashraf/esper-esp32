idf_component_register(SRCS "filesystem.cpp" INCLUDE_DIRS "."
    PRIV_REQUIRES error settings app_update littlefs
    EMBED_FILES files/defaultsettings.json files/homepage.html files/app_scripts.js files/stylesheet.css)
