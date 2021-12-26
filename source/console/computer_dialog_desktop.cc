//
// Aspia Project
// Copyright (C) 2020 Dmitry Chapyshev <dmitry@aspia.ru>
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <https://www.gnu.org/licenses/>.
//

#include "console/computer_dialog_desktop.h"

#include "base/logging.h"
#include "base/desktop/pixel_format.h"

namespace console {

namespace {

enum ColorDepth
{
    COLOR_DEPTH_ARGB,
    COLOR_DEPTH_RGB565,
    COLOR_DEPTH_RGB332,
    COLOR_DEPTH_RGB222,
    COLOR_DEPTH_RGB111
};

base::PixelFormat parsePixelFormat(const proto::PixelFormat& format)
{
    return base::PixelFormat(
        static_cast<uint8_t>(format.bits_per_pixel()),
        static_cast<uint16_t>(format.red_max()),
        static_cast<uint16_t>(format.green_max()),
        static_cast<uint16_t>(format.blue_max()),
        static_cast<uint8_t>(format.red_shift()),
        static_cast<uint8_t>(format.green_shift()),
        static_cast<uint8_t>(format.blue_shift()));
}

void serializePixelFormat(const base::PixelFormat& from, proto::PixelFormat* to)
{
    to->set_bits_per_pixel(from.bitsPerPixel());

    to->set_red_max(from.redMax());
    to->set_green_max(from.greenMax());
    to->set_blue_max(from.blueMax());

    to->set_red_shift(from.redShift());
    to->set_green_shift(from.greenShift());
    to->set_blue_shift(from.blueShift());
}

} // namespace

ComputerDialogDesktop::ComputerDialogDesktop(int type, QWidget* parent)
    : ComputerDialogTab(type, parent)
{
    ui.setupUi(this);

    connect(ui.combo_codec, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ComputerDialogDesktop::onCodecChanged);

    connect(ui.slider_compress_ratio, &QSlider::valueChanged,
            this, &ComputerDialogDesktop::onCompressionRatioChanged);
}

void ComputerDialogDesktop::restoreSettings(
    proto::SessionType session_type, const proto::DesktopConfig& config)
{
    QComboBox* combo_codec = ui.combo_codec;
    combo_codec->addItem(QStringLiteral("VP9"), proto::VIDEO_ENCODING_VP9);
    combo_codec->addItem(QStringLiteral("VP8"), proto::VIDEO_ENCODING_VP8);
    combo_codec->addItem(QStringLiteral("ZSTD"), proto::VIDEO_ENCODING_ZSTD);

    QComboBox* combo_color_depth = ui.combobox_color_depth;
    combo_color_depth->addItem(tr("True color (32 bit)"), COLOR_DEPTH_ARGB);
    combo_color_depth->addItem(tr("High color (16 bit)"), COLOR_DEPTH_RGB565);
    combo_color_depth->addItem(tr("256 colors (8 bit)"), COLOR_DEPTH_RGB332);
    combo_color_depth->addItem(tr("64 colors (6 bit)"), COLOR_DEPTH_RGB222);
    combo_color_depth->addItem(tr("8 colors (3 bit)"), COLOR_DEPTH_RGB111);

    int current_codec = combo_codec->findData(config.video_encoding());
    if (current_codec == -1)
        current_codec = 0;

    combo_codec->setCurrentIndex(current_codec);
    onCodecChanged(current_codec);

    base::PixelFormat pixel_format = parsePixelFormat(config.pixel_format());
    ColorDepth color_depth;

    if (pixel_format.isEqual(base::PixelFormat::ARGB()))
        color_depth = COLOR_DEPTH_ARGB;
    else if (pixel_format.isEqual(base::PixelFormat::RGB565()))
        color_depth = COLOR_DEPTH_RGB565;
    else if (pixel_format.isEqual(base::PixelFormat::RGB332()))
        color_depth = COLOR_DEPTH_RGB332;
    else if (pixel_format.isEqual(base::PixelFormat::RGB222()))
        color_depth = COLOR_DEPTH_RGB222;
    else if (pixel_format.isEqual(base::PixelFormat::RGB111()))
        color_depth = COLOR_DEPTH_RGB111;
    else
        color_depth = COLOR_DEPTH_ARGB;

    int current_color_depth = combo_color_depth->findData(color_depth);
    if (current_color_depth != -1)
        combo_color_depth->setCurrentIndex(current_color_depth);

    ui.slider_compress_ratio->setValue(static_cast<int>(config.compress_ratio()));
    onCompressionRatioChanged(static_cast<int>(config.compress_ratio()));

    if (config.audio_encoding() != proto::AUDIO_ENCODING_UNKNOWN)
        ui.checkbox_audio->setChecked(true);

    if (session_type == proto::SESSION_TYPE_DESKTOP_MANAGE)
    {
        if (config.flags() & proto::LOCK_AT_DISCONNECT)
            ui.checkbox_lock_at_disconnect->setChecked(true);

        if (config.flags() & proto::BLOCK_REMOTE_INPUT)
            ui.checkbox_block_remote_input->setChecked(true);

        if (config.flags() & proto::ENABLE_CURSOR_SHAPE)
            ui.checkbox_cursor_shape->setChecked(true);

        if (config.flags() & proto::ENABLE_CLIPBOARD)
            ui.checkbox_clipboard->setChecked(true);
    }
    else
    {
        ui.groupbox_other->hide();
        ui.checkbox_cursor_shape->hide();
        ui.checkbox_clipboard->hide();
    }

    if (config.flags() & proto::DISABLE_DESKTOP_EFFECTS)
        ui.checkbox_desktop_effects->setChecked(true);

    if (config.flags() & proto::DISABLE_DESKTOP_WALLPAPER)
        ui.checkbox_desktop_wallpaper->setChecked(true);

    if (config.flags() & proto::DISABLE_FONT_SMOOTHING)
        ui.checkbox_font_smoothing->setChecked(true);
}

void ComputerDialogDesktop::saveSettings(proto::DesktopConfig* config)
{
    proto::VideoEncoding video_encoding =
        static_cast<proto::VideoEncoding>(ui.combo_codec->currentData().toInt());

    config->set_video_encoding(video_encoding);

    if (video_encoding == proto::VIDEO_ENCODING_ZSTD)
    {
        base::PixelFormat pixel_format;

        switch (ui.combobox_color_depth->currentData().toInt())
        {
            case COLOR_DEPTH_ARGB:
                pixel_format = base::PixelFormat::ARGB();
                break;

            case COLOR_DEPTH_RGB565:
                pixel_format = base::PixelFormat::RGB565();
                break;

            case COLOR_DEPTH_RGB332:
                pixel_format = base::PixelFormat::RGB332();
                break;

            case COLOR_DEPTH_RGB222:
                pixel_format = base::PixelFormat::RGB222();
                break;

            case COLOR_DEPTH_RGB111:
                pixel_format = base::PixelFormat::RGB111();
                break;

            default:
                DLOG(LS_FATAL) << "Unexpected color depth";
                break;
        }

        serializePixelFormat(pixel_format, config->mutable_pixel_format());

        config->set_compress_ratio(static_cast<uint32_t>(ui.slider_compress_ratio->value()));
    }

    uint32_t flags = 0;

    if (ui.checkbox_audio->isChecked())
        config->set_audio_encoding(proto::AUDIO_ENCODING_OPUS);
    else
        config->set_audio_encoding(proto::AUDIO_ENCODING_UNKNOWN);

    if (ui.checkbox_cursor_shape->isChecked() && ui.checkbox_cursor_shape->isEnabled())
        flags |= proto::ENABLE_CURSOR_SHAPE;

    if (ui.checkbox_clipboard->isChecked() && ui.checkbox_clipboard->isEnabled())
        flags |= proto::ENABLE_CLIPBOARD;

    if (ui.checkbox_desktop_effects->isChecked())
        flags |= proto::DISABLE_DESKTOP_EFFECTS;

    if (ui.checkbox_desktop_wallpaper->isChecked())
        flags |= proto::DISABLE_DESKTOP_WALLPAPER;

    if (ui.checkbox_font_smoothing->isChecked())
        flags |= proto::DISABLE_FONT_SMOOTHING;

    if (ui.checkbox_block_remote_input->isChecked())
        flags |= proto::BLOCK_REMOTE_INPUT;

    if (ui.checkbox_lock_at_disconnect->isChecked())
        flags |= proto::LOCK_AT_DISCONNECT;

    config->set_flags(flags);
}

void ComputerDialogDesktop::onCodecChanged(int item_index)
{
    bool has_pixel_format =
        (ui.combo_codec->itemData(item_index).toInt() == proto::VIDEO_ENCODING_ZSTD);

    ui.label_color_depth->setEnabled(has_pixel_format);
    ui.combobox_color_depth->setEnabled(has_pixel_format);
    ui.label_compress_ratio->setEnabled(has_pixel_format);
    ui.slider_compress_ratio->setEnabled(has_pixel_format);
    ui.label_fast->setEnabled(has_pixel_format);
    ui.label_best->setEnabled(has_pixel_format);
}

void ComputerDialogDesktop::onCompressionRatioChanged(int value)
{
    ui.label_compress_ratio->setText(tr("Compression ratio: %1").arg(value));
}

} // namespace console
