# ##############################################################################
# A Logan Thomas Barnes project
# ##############################################################################

# Graphics APIs
find_package( Vulkan REQUIRED )

cpmaddpackage(
  NAME
  LtbVlk
  GITHUB_REPOSITORY
  LoganBarnes/ltb-vlk
  GIT_TAG
  e6bf57b
)

# Macos:
# brew install pkg-config
# brew install libusb
#
# Ubuntu:
# sudo apt install pkg-config libusb-1.0-0-dev
cpmaddpackage(
  NAME
  hackrf
  GITHUB_REPOSITORY
  greatscottgadgets/hackrf
  GIT_TAG
  v2026.01.3
  SOURCE_SUBDIR host
)
