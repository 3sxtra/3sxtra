################################################################################
#
# 3sx - Street Fighter III: 3rd Strike port
#
################################################################################

3SX_VERSION = main
3SX_SITE = local
3SX_SITE_METHOD = local
3SX_LICENSE = Proprietary
3SX_DEPENDENCIES = sdl3 zlib cmake host-cmake miniupnpc

# Sync source from the repo root
define 3SX_RSYNC_SOURCE
	rsync -a --exclude=.git --exclude=third_party/sdl3/build \
		--exclude=third_party/sdl3_image/build \
		--exclude=build \
		$(3SX_SITE)/ $(@D)/
endef

3SX_EXTRACT_CMDS = $(3SX_RSYNC_SOURCE)

# Download dependencies before configure
define 3SX_DOWNLOAD_DEPS
	cd $(@D) && bash tools/batocera/rpi4/download-deps_rpi4.sh
endef

3SX_PRE_CONFIGURE_HOOKS += 3SX_DOWNLOAD_DEPS

# Build dependencies using cross-compiler
define 3SX_BUILD_DEPS
	cd $(@D) && \
		TOOLCHAIN_DIR=$(HOST_DIR) \
		CROSS_PREFIX=$(TARGET_CROSS) \
		SYSROOT=$(STAGING_DIR) \
		bash tools/batocera/rpi4/build-deps_rpi4.sh
endef

3SX_PRE_CONFIGURE_HOOKS += 3SX_BUILD_DEPS

# CMake configuration
3SX_CONF_OPTS = \
	-DCMAKE_BUILD_TYPE=Release \
	-DPLATFORM_RPI4=ON \
	-DENABLE_TESTS=OFF \
	-DCMAKE_C_COMPILER=$(TARGET_CC) \
	-DCMAKE_CXX_COMPILER=$(TARGET_CXX) \
	-DCMAKE_SYSROOT=$(STAGING_DIR)

# Install to target
define 3SX_INSTALL_TARGET_CMDS
	$(INSTALL) -D $(@D)/build/3sx $(TARGET_DIR)/usr/games/3sx/3sx
	cp -r $(@D)/assets $(TARGET_DIR)/usr/games/3sx/
	cp -r $(@D)/src/shaders $(TARGET_DIR)/usr/games/3sx/
	cp -r $(@D)/third_party/slang-shaders $(TARGET_DIR)/usr/games/3sx/shaders/libretro
	$(INSTALL) -D $(@D)/tools/batocera/rpi4/3sx.sh $(TARGET_DIR)/usr/games/3sx/3sx.sh
endef

# Package for deployment
define 3SX_DEPLOY
	cd $(@D) && bash tools/batocera/rpi4/deploy.sh
endef

3SX_POST_INSTALL_TARGET_HOOKS += 3SX_DEPLOY

$(eval $(cmake-package))
