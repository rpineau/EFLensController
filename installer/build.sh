#!/bin/bash

mkdir -p ROOT/tmp/EFLensController_X2/
cp "../EFLensController.ui" ROOT/tmp/EFLensController_X2/
cp "../lens.txt" ROOT/tmp/EFLensController_X2/
cp "../focuserlist EFLensController.txt" ROOT/tmp/EFLensController_X2/
cp "../build/Release/libEFLensController.dylib" ROOT/tmp/EFLensController_X2/

if [ ! -z "$installer_signature" ]; then
# signed package using env variable installer_signature
pkgbuild --root ROOT --identifier org.rti-zone.EFLensController_X2 --sign "$installer_signature" --scripts Scripts --version 1.0 EFLensController_X2.pkg
pkgutil --check-signature ./EFLensController_X2.pkg
else
pkgbuild --root ROOT --identifier org.rti-zone.EFLensController_X2 --scripts Scripts --version 1.0 EFLensController_X2.pkg
fi

rm -rf ROOT
