// Pixel bounding box of the TFT display within badge-front-cutout.webp
// (916x1045 canvas), measured directly against the real photo. Used to
// composite firmware screenshots onto the actual display region instead
// of a generic device-frame mockup.
export const BADGE_IMAGE_SIZE = { width: 916, height: 1045 };

export const DISPLAY_BOX = {
  left: 306,
  top: 552,
  right: 598,
  bottom: 754,
};

export const displayBoxPercent = () => ({
  left: (DISPLAY_BOX.left / BADGE_IMAGE_SIZE.width) * 100,
  top: (DISPLAY_BOX.top / BADGE_IMAGE_SIZE.height) * 100,
  width: ((DISPLAY_BOX.right - DISPLAY_BOX.left) / BADGE_IMAGE_SIZE.width) * 100,
  height: ((DISPLAY_BOX.bottom - DISPLAY_BOX.top) / BADGE_IMAGE_SIZE.height) * 100,
});
