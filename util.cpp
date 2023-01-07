/*
 * mcu-app - Microcontroller application framework
 * Copyright 2022-2023  Simon Arlott
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "util.h"

#include <Arduino.h>

#ifdef ARDUINO_ARCH_ESP32
# include <esp_ota_ops.h>
# include <rom/rtc.h>
#endif

#include <string>
#include <vector>

#include <CBOR.h>
#include <CBOR_parsing.h>
#include <CBOR_streams.h>

#include <uuid/common.h>

namespace cbor = qindesign::cbor;

namespace app {

HexPrintable::HexPrintable(const uint8_t *buf, size_t len)
		: buf_(buf), len_(len) {
}

size_t HexPrintable::printTo(Print &print) const {
	for (size_t i = 0; i < len_; i++) {
		print.printf(uuid::read_flash_string(F("%02x")).c_str(), buf_[i]);
	}
	return len_;
}

std::string hex_string(const uint8_t *buf, size_t len) {
	std::vector<char> text(2 * len + 1);

	for (size_t i = 0; i < len; i++) {
		snprintf(&text[2 * i], 3, uuid::read_flash_string(F("%02x")).c_str(), buf[i]);
	}

	return text.data();
}

std::string normalise_filename(const std::string &filename) {
	std::string output;

	output.reserve(filename.length());

	for (size_t i = 0; i < filename.length(); i++) {
		if (output.empty() || output.back() == '/') {
			if (filename[i] == '/' && !output.empty()) {
				continue;
			} else if (filename[i] == '.') {
				if (filename.length() - i < 2 || filename[i + 1] == '/') {
					i++;
					continue;
				} else if (filename[i + 1] == '.' && (filename.length() - i < 3 || filename[i + 2] == '/')) {
					i += 2;

					size_t pos = output.rfind('/', output.length() - 2);
					if (pos != std::string::npos) {
						output.resize(pos + 1);
					} else {
						output.resize(0);
					}
					continue;
				}
			}
		}

		size_t pos = filename.find('/', i);
		if (pos != std::string::npos) {
			pos++;
		} else {
			pos = filename.length();
		}

		output.insert(output.end(), &filename[i], &filename[pos]);
		i = pos - 1;
	}

	return output;
}

std::string base_filename(const std::string &filename) {
	std::string path = normalise_filename(filename);

	if (!path.empty() && path.back() == '/')
		path.pop_back();

	size_t pos = filename.rfind('/');
	if (pos != std::string::npos) {
		return filename.substr(pos + 1);
	} else {
		return filename;
	}
}

static void write_text(cbor::Writer &writer, const char *text, size_t length) {
	writer.beginText(length);
	writer.writeBytes(reinterpret_cast<const uint8_t*>(text), length);
}

void write_text(cbor::Writer &writer, const std::string &text) {
	write_text(writer, text.c_str()), text.length();
}

void write_text(cbor::Writer &writer, const char *text) {
	write_text(writer, text, strlen(text));
}

void write_text(cbor::Writer &writer, const __FlashStringHelper *text) {
	write_text(writer, uuid::read_flash_string(text));
}

bool read_text(cbor::Reader &reader, std::string &text, size_t max_length) {
	uint64_t length;
	bool indefinite;

	if (!cbor::expectText(reader, &length, &indefinite) || indefinite)
		return false;

	if (length > max_length)
		return false;

	std::vector<char> data(length + 1);

	if (cbor::readFully(reader, reinterpret_cast<uint8_t*>(data.data()), length) != length)
		return false;

	text = {data.data()};
	return true;
}

bool expect_float(cbor::Reader &reader, float &value) {
	auto data_type = reader.readDataType();

	if (data_type == cbor::DataType::kFloat) {
		value = reader.getFloat();
	} else if (data_type == cbor::DataType::kDouble) {
		value = reader.getDouble();
	} else if (data_type == cbor::DataType::kUnsignedInt) {
		value = reader.getUnsignedInt();
	} else if (data_type == cbor::DataType::kNegativeInt) {
		value = reader.getInt();
	} else {
		return false;
	}

	return true;
}

#ifdef ARDUINO_ARCH_ESP32
std::string reset_reason_string(RESET_REASON reason) {
	const __FlashStringHelper *text = F("unknown");

	switch (reason) {
	case NO_MEAN: break;
#define RR_STR(reason) case reason ## _RESET: text = F(#reason); break;
	RR_STR(POWERON)
	RR_STR(RTC_SW_SYS)
	RR_STR(DEEPSLEEP)
	RR_STR(TG0WDT_SYS)
	RR_STR(TG1WDT_SYS)
	RR_STR(RTCWDT_SYS)
	RR_STR(INTRUSION)
	RR_STR(TG0WDT_CPU)
	RR_STR(RTC_SW_CPU)
	RR_STR(RTCWDT_CPU)
	RR_STR(RTCWDT_BROWN_OUT)
	RR_STR(RTCWDT_RTC)
	RR_STR(TG1WDT_CPU)
	RR_STR(SUPER_WDT)
	RR_STR(GLITCH_RTC)
	RR_STR(EFUSE)
#undef RR_STR
	}

	return uuid::read_flash_string(text);
}

std::string wakeup_cause_string(WAKEUP_REASON cause) {
	std::string text;

	while (static_cast<unsigned long>(cause) != 0) {
		int bit = __builtin_ffsl(cause);
		unsigned long value = 1UL << (bit - 1);

		switch (static_cast<WAKEUP_REASON>(value)) {
		case NO_SLEEP: break;
#define WR_STR2(reason, desc) case reason: text.append(uuid::read_flash_string(F(desc " "))); break;
#define WR_STRT(reason) WR_STR2(reason ## _TRIG, #reason)
#define WR_STR(reason) WR_STR2(reason, #reason)
		WR_STRT(EXT_EVENT0)
		WR_STRT(EXT_EVENT1)
		WR_STRT(GPIO)
		WR_STR(TIMER_EXPIRE)
		WR_STRT(SDIO)
		WR_STRT(MAC)
		WR_STRT(UART0)
		WR_STRT(UART1)
		WR_STRT(TOUCH)
		WR_STRT(SAR)
		WR_STRT(BT)
		WR_STRT(RISCV)
		WR_STRT(XTAL_DEAD)
		WR_STRT(RISCV_TRAP)
		WR_STRT(USB)
#undef WR_STR
#undef WR_STRT
#undef WR_STR2
		}

		cause = static_cast<WAKEUP_REASON>(static_cast<unsigned long>(cause) & ~value);
	}

	if (!text.empty()) {
		text.pop_back();
	}

	return text;
}
#endif

#if !defined(ENV_NATIVE) && !defined(ARDUINO_ARCH_ESP8266)
const __FlashStringHelper *ota_state_string(esp_ota_img_states_t state) {
	switch (state) {
	case ESP_OTA_IMG_NEW: return F("new");
	case ESP_OTA_IMG_PENDING_VERIFY: return F("pending-verify");
	case ESP_OTA_IMG_VALID: return F("valid");
	case ESP_OTA_IMG_INVALID: return F("invalid");
	case ESP_OTA_IMG_ABORTED: return F("aborted");
	case ESP_OTA_IMG_UNDEFINED: return F("undefined");
	}

	return F("unknown");
}
#endif

} // namespace app
