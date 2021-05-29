/***************************************************************************
 *   Copyright (C) 2020 by Federico Amedeo Izzo IU2NUO,                    *
 *                         Niccolò Izzo IU2KIN                             *
 *                         Frederik Saraci IU2NRO                          *
 *                         Silvano Seva IU2KWO                             *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 3 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   As a special exception, if other files instantiate templates or use   *
 *   macros or inline functions from this file, or you compile this file   *
 *   and link it with other works to produce a work based on this file,    *
 *   this file does not by itself cause the resulting work to be covered   *
 *   by the GNU General Public License. However the source code for this   *
 *   file must still be made available in accordance with the GNU General  *
 *   Public License. This exception does not invalidate any other reasons  *
 *   why a work based on this file might be covered by the GNU General     *
 *   Public License.                                                       *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, see <http://www.gnu.org/licenses/>   *
 ***************************************************************************/

#include <interfaces/platform.h>
#include <interfaces/radio.h>
#include <interfaces/gpio.h>
#include <calibInfo_MDx.h>
#include <calibUtils.h>
#include <hwconfig.h>
#include <algorithm>
#include "HR_C6000.h"
#include "AT1846S.h"

const mduv3x0Calib_t *calData;  // Pointer to calibration data
const rtxStatus_t    *config;   // Pointer to data structure with radio configuration

int8_t  currRxBand = -1;        // Current band for RX
int8_t  currTxBand = -1;        // Current band for TX
uint8_t txpwr_lo = 0;           // APC voltage for TX output power control, low power
uint8_t txpwr_hi = 0;           // APC voltage for TX output power control, high power

enum opstatus radioStatus;      // Current operating status

HR_C6000& C6000  = HR_C6000::instance();  // HR_C5000 driver
AT1846S& at1846s = AT1846S::instance();   // AT1846S driver

/**
 * \internal
 * Function to identify the current band (VHF or UHF), given an input frequency.
 *
 * @param freq frequency in Hz.
 * @return 0 if the frequency is in the VHF band,
 *         1 if the frequency is in the UHF band,
 *        -1 if the band to which the frequency belongs is neither VHF nor UHF.
 */
int8_t _getBandFromFrequency(freq_t freq)
{
    if((freq >= FREQ_LIMIT_VHF_LO) && (freq <= FREQ_LIMIT_VHF_HI)) return 0;
    if((freq >= FREQ_LIMIT_UHF_LO) && (freq <= FREQ_LIMIT_UHF_HI)) return 1;
    return -1;
}

void radio_init(const rtxStatus_t *rtxState)
{
    /*
     * Load calibration data
     */
    calData = reinterpret_cast< const mduv3x0Calib_t * >(platform_getCalibrationData());

    config      = rtxState;
    radioStatus = OFF;

    /*
     * Configure RTX GPIOs
     */
    gpio_setMode(VHF_LNA_EN,   OUTPUT);
    gpio_setMode(UHF_LNA_EN,   OUTPUT);
    gpio_setMode(PA_EN_1,      OUTPUT);
    gpio_setMode(PA_EN_2,      OUTPUT);
    gpio_setMode(PA_SEL_SW,    OUTPUT);

    gpio_clearPin(VHF_LNA_EN);
    gpio_clearPin(UHF_LNA_EN);
    gpio_clearPin(PA_EN_1);
    gpio_clearPin(PA_EN_2);
    gpio_clearPin(PA_SEL_SW);

    /* TODO: keep audio connected to HR_C6000, for volume control */
    gpio_setMode(RX_AUDIO_MUX, OUTPUT);
    gpio_setPin(RX_AUDIO_MUX);

    /*
     * Configure and enable DAC
     */
    gpio_setMode(APC_REF, INPUT_ANALOG);

    RCC->APB1ENR |= RCC_APB1ENR_DACEN;
    DAC->CR = DAC_CR_EN1;
    DAC->DHR12R1 = 0;

    /*
     * Configure AT1846S and HR_C6000
     */
    at1846s.init();
    C6000.init();
}

void radio_terminate()
{
    radio_disableRtx();
    C6000.terminate();
    at1846s.terminate();

    DAC->DHR12R1 = 0;
    RCC->APB1ENR &= ~RCC_APB1ENR_DACEN;
}

void radio_setOpmode(const enum opmode mode)
{
    switch(mode)
    {
        case FM:
            at1846s.setOpMode(AT1846S_OpMode::FM);
            C6000.fmMode();
            break;

        case DMR:
            at1846s.setOpMode(AT1846S_OpMode::DMR);
//             C6000.dmrMode();
            break;

        case M17:
            // TODO
            break;

        default:
            break;
    }
}

bool radio_checkRxDigitalSquelch()
{
    return true;
}

void radio_enableRx()
{
    gpio_clearPin(PA_EN_1);
    gpio_clearPin(PA_EN_2);
    gpio_clearPin(VHF_LNA_EN);
    gpio_clearPin(UHF_LNA_EN);
    DAC->DHR12R1 = 0;

    if(currRxBand < 0) return;

    at1846s.setFrequency(config->rxFrequency);
    at1846s.setFuncMode(AT1846S_FuncMode::RX);

    if(currRxBand == 0)
    {
        gpio_setPin(VHF_LNA_EN);
    }
    else
    {
        gpio_setPin(UHF_LNA_EN);
    }

    radioStatus = RX;
}

void radio_enableTx()
{
    if(config->txDisable == 1) return;

    gpio_clearPin(VHF_LNA_EN);
    gpio_clearPin(UHF_LNA_EN);
    gpio_clearPin(PA_EN_1);
    gpio_clearPin(PA_EN_2);

    at1846s.setFrequency(config->txFrequency);

    // Constrain output power between 1W and 5W.
    float power  = std::max(std::min(config->txPower, 5.0f), 1.0f);
    float pwrHi  = static_cast< float >(txpwr_hi);
    float pwrLo  = static_cast< float >(txpwr_lo);
    float apc    = pwrLo + (pwrHi - pwrLo)/4.0f*(power - 1.0f);
    DAC->DHR12L1 = static_cast< uint8_t >(apc) * 0xFF;

    switch(config->opMode)
    {
        case FM:
        {
            FmConfig cfg = (config->bandwidth == BW_12_5) ? FmConfig::BW_12p5kHz
                                                          : FmConfig::BW_25kHz;
            C6000.startAnalogTx(TxAudioSource::MIC, cfg | FmConfig::PREEMPH_EN);
        }
            break;

        case M17:
            C6000.startAnalogTx(TxAudioSource::LINE_IN, FmConfig::BW_25kHz);
            break;

        default:
            break;
    }

    at1846s.setFuncMode(AT1846S_FuncMode::TX);

    gpio_setPin(PA_EN_1);

    if(currTxBand == 0)
    {
        gpio_clearPin(PA_SEL_SW);
    }
    else
    {
        gpio_setPin(PA_SEL_SW);
    }

    gpio_setPin(PA_EN_2);

    if(config->txToneEn)
    {
        at1846s.enableTxCtcss(config->txTone);
    }

    radioStatus = TX;
}

void radio_disableRtx()
{
    gpio_clearPin(VHF_LNA_EN);
    gpio_clearPin(UHF_LNA_EN);
    gpio_clearPin(PA_EN_1);
    gpio_clearPin(PA_EN_2);
    DAC->DHR12L1 = 0;

    // If we are currently transmitting, stop tone and C6000 TX
    if(radioStatus == TX)
    {
        at1846s.disableCtcss();
        C6000.stopAnalogTx();
    }

    at1846s.setFuncMode(AT1846S_FuncMode::OFF);
    radioStatus = OFF;
}

void radio_updateConfiguration()
{
    currRxBand = _getBandFromFrequency(config->rxFrequency);
    currTxBand = _getBandFromFrequency(config->txFrequency);

    if((currRxBand < 0) || (currTxBand < 0)) return;

    /* TCXO bias voltage */
    uint8_t modBias = calData->vhfCal.freqAdjustMid;
    if(currRxBand > 0) modBias = calData->uhfCal.freqAdjustMid;
    C6000.setModOffset(modBias);

    /*
     * Discarding "const" qualifier to suppress compiler warnings.
     * This operation is safe anyway because calibration data is only read.
     */
    mduv3x0Calib_t *cal  = const_cast< mduv3x0Calib_t * >(calData);
    freq_t  *txCalPoints = cal->vhfCal.txFreq;
    uint8_t *loPwrCal    = cal->vhfCal.txLowPower;
    uint8_t *hiPwrCal    = cal->vhfCal.txHighPower;
    uint8_t *qRangeCal   = (config->opMode == FM) ? cal->vhfCal.analogSendQrange
                                                  : cal->vhfCal.sendQrange;
    if(currTxBand > 0)
    {
        txCalPoints = cal->uhfCal.txFreq;
        loPwrCal    = cal->uhfCal.txLowPower;
        hiPwrCal    = cal->uhfCal.txHighPower;
        qRangeCal   = (config->opMode == FM) ? cal->uhfCal.analogSendQrange
                                             : cal->uhfCal.sendQrange;
    }

    /* APC voltage for TX output power control */
    txpwr_lo = interpCalParameter(config->txFrequency, txCalPoints, loPwrCal, 9);
    txpwr_hi = interpCalParameter(config->txFrequency, txCalPoints, hiPwrCal, 9);

    /* HR_C6000 modulation amplitude */
    uint8_t Q = interpCalParameter(config->txFrequency, txCalPoints, qRangeCal, 9);
    C6000.setModAmplitude(0, Q);

    // Set bandwidth, force 12.5kHz for DMR mode
//     enum bandwidth bandwidth = static_cast< enum bandwidth >(config->bandwidth);
//     if((bandwidth == BW_12_5) || (config->opMode == DMR))
//     {
//         at1846s.setBandwidth(AT1846S_BW::_12P5);
//     }
//     else
//     {
        at1846s.setBandwidth(AT1846S_BW::_25);
//     }

    /*
     * Update VCO frequency and tuning parameters if current operating status
     * is different from OFF.
     * This is done by calling again the corresponding functions, which is safe
     * to do and avoids code duplication.
     */
    if(radioStatus == RX) radio_enableRx();
    if(radioStatus == TX) radio_enableTx();
}

float radio_getRssi()
{
    return static_cast< float > (at1846s.readRSSI());
}

enum opstatus radio_getStatus()
{
    return radioStatus;
}
