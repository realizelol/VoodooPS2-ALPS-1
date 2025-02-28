/*
 * Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.2 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License
 * http://www.apple.com/publicsource and read it before using this file.
 *
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 *
 * @APPLE_LICENSE_HEADER_END@
 */

#include <IOKit/IOService.h>

#include "alps.h"
#include <IOKit/IOLib.h>
#include <IOKit/hidsystem/IOHIDParameter.h>
#include <IOKit/IOWorkLoop.h>
#include <IOKit/IOTimerEventSource.h>
#include <IOKit/usb/IOUSBHostFamily.h>
#include <IOKit/usb/IOUSBHostHIDDevice.h>
#include <IOKit/bluetooth/BluetoothAssignedNumbers.h>
#include "VoodooPS2Controller.h"
#include "VoodooInputMultitouch/VoodooInputTransducer.h"
#include "VoodooInputMultitouch/VoodooInputMessages.h"

#undef NULL
#define NULL 0

enum {
    kTapEnabled = 0x01
};

#define ARRAY_SIZE(x)    (sizeof(x)/sizeof(x[0]))
#define abs(x) ((x) < 0 ? -(x) : (x))
#define BIT(x) (1 << (x))


/* ============================================================================================== */
/* ===============================||\\ alps.c Definitions //||=================================== */
/* ============================================================================================== */

/*
 * Definitions for ALPS version 3 and 4 command mode protocol
 */
#define ALPS_CMD_NIBBLE_10  0x01f2

#define ALPS_REG_BASE_RUSHMORE  0xc2c0
#define ALPS_REG_BASE_V7	0xc2c0
#define ALPS_REG_BASE_PINNACLE  0x0000

static const struct alps_nibble_commands alps_v3_nibble_commands[] = {
    { kDP_MouseSetPoll,                 0x00 }, /* 0 no send/recv */
    { kDP_SetDefaults,                  0x00 }, /* 1 no send/recv */
    { kDP_SetMouseScaling2To1,          0x00 }, /* 2 no send/recv */
    { kDP_SetMouseSampleRate | 0x1000,  0x0a }, /* 3 send=1 recv=0 */
    { kDP_SetMouseSampleRate | 0x1000,  0x14 }, /* 4 ..*/
    { kDP_SetMouseSampleRate | 0x1000,  0x28 }, /* 5 ..*/
    { kDP_SetMouseSampleRate | 0x1000,  0x3c }, /* 6 ..*/
    { kDP_SetMouseSampleRate | 0x1000,  0x50 }, /* 7 ..*/
    { kDP_SetMouseSampleRate | 0x1000,  0x64 }, /* 8 ..*/
    { kDP_SetMouseSampleRate | 0x1000,  0xc8 }, /* 9 ..*/
    { kDP_CommandNibble10    | 0x0100,  0x00 }, /* a send=0 recv=1 */
    { kDP_SetMouseResolution | 0x1000,  0x00 }, /* b send=1 recv=0 */
    { kDP_SetMouseResolution | 0x1000,  0x01 }, /* c ..*/
    { kDP_SetMouseResolution | 0x1000,  0x02 }, /* d ..*/
    { kDP_SetMouseResolution | 0x1000,  0x03 }, /* e ..*/
    { kDP_SetMouseScaling1To1,          0x00 }, /* f no send/recv */
};

static const struct alps_nibble_commands alps_v4_nibble_commands[] = {
    { kDP_Enable,                       0x00 }, /* 0 no send/recv */
    { kDP_SetDefaults,                  0x00 }, /* 1 no send/recv */
    { kDP_SetMouseScaling2To1,          0x00 }, /* 2 no send/recv */
    { kDP_SetMouseSampleRate | 0x1000,  0x0a }, /* 3 send=1 recv=0 */
    { kDP_SetMouseSampleRate | 0x1000,  0x14 }, /* 4 ..*/
    { kDP_SetMouseSampleRate | 0x1000,  0x28 }, /* 5 ..*/
    { kDP_SetMouseSampleRate | 0x1000,  0x3c }, /* 6 ..*/
    { kDP_SetMouseSampleRate | 0x1000,  0x50 }, /* 7 ..*/
    { kDP_SetMouseSampleRate | 0x1000,  0x64 }, /* 8 ..*/
    { kDP_SetMouseSampleRate | 0x1000,  0xc8 }, /* 9 ..*/
    { kDP_CommandNibble10    | 0x0100,  0x00 }, /* a send=0 recv=1 */
    { kDP_SetMouseResolution | 0x1000,  0x00 }, /* b send=1 recv=0 */
    { kDP_SetMouseResolution | 0x1000,  0x01 }, /* c ..*/
    { kDP_SetMouseResolution | 0x1000,  0x02 }, /* d ..*/
    { kDP_SetMouseResolution | 0x1000,  0x03 }, /* e ..*/
    { kDP_SetMouseScaling1To1,          0x00 }, /* f no send/recv */
};

static const struct alps_nibble_commands alps_v6_nibble_commands[] = {
    { kDP_Enable,		            0x00 }, /* 0 */
    { kDP_SetMouseSampleRate,		0x0a }, /* 1 */
    { kDP_SetMouseSampleRate,		0x14 }, /* 2 */
    { kDP_SetMouseSampleRate,		0x28 }, /* 3 */
    { kDP_SetMouseSampleRate,		0x3c }, /* 4 */
    { kDP_SetMouseSampleRate,		0x50 }, /* 5 */
    { kDP_SetMouseSampleRate,		0x64 }, /* 6 */
    { kDP_SetMouseSampleRate,		0xc8 }, /* 7 */
    { kDP_GetId,		            0x00 }, /* 8 */
    { kDP_GetMouseInformation,		0x00 }, /* 9 */
    { kDP_SetMouseResolution,		0x00 }, /* a */
    { kDP_SetMouseResolution,		0x01 }, /* b */
    { kDP_SetMouseResolution,		0x02 }, /* c */
    { kDP_SetMouseResolution,		0x03 }, /* d */
    { kDP_SetMouseScaling2To1,	    0x00 }, /* e */
    { kDP_SetMouseScaling1To1,	    0x00 }, /* f */
};


#define ALPS_DUALPOINT          0x02    /* touchpad has trackstick */
#define ALPS_PASS               0x04    /* device has a pass-through port */

#define ALPS_WHEEL              0x08    /* hardware wheel present */
#define ALPS_FW_BK_1            0x10    /* front & back buttons present */
#define ALPS_FW_BK_2            0x20    /* front & back buttons present */
#define ALPS_FOUR_BUTTONS       0x40    /* 4 direction button present */
#define ALPS_PS2_INTERLEAVED    0x80    /* 3-byte PS/2 packet interleaved with 6-byte ALPS packet */
#define ALPS_STICK_BITS		    0x100	/* separate stick button bits */
#define ALPS_BUTTONPAD		    0x200	/* device is a clickpad */
#define ALPS_DUALPOINT_WITH_PRESSURE	0x400	/* device can report trackpoint pressure */


static const struct alps_model_info alps_model_data[] = {
    /*
     * XXX This entry is suspicious. First byte has zero lower nibble,
     * which is what a normal mouse would report. Also, the value 0x0e
     * isn't valid per PS/2 spec.
     */
    { { 0x20, 0x02, 0x0e }, { ALPS_PROTO_V2, 0xf8, 0xf8, ALPS_PASS | ALPS_DUALPOINT } },

    { { 0x22, 0x02, 0x0a }, { ALPS_PROTO_V2, 0xf8, 0xf8, ALPS_PASS | ALPS_DUALPOINT } },
    { { 0x22, 0x02, 0x14 }, { ALPS_PROTO_V2, 0xff, 0xff, ALPS_PASS | ALPS_DUALPOINT } },    /* Dell Latitude D600 */
    { { 0x32, 0x02, 0x14 }, { ALPS_PROTO_V2, 0xf8, 0xf8, ALPS_PASS | ALPS_DUALPOINT } },    /* Toshiba Salellite Pro M10 */
    { { 0x33, 0x02, 0x0a }, { ALPS_PROTO_V1, 0x88, 0xf8, 0 } },                /* UMAX-530T */
    { { 0x52, 0x01, 0x14 }, { ALPS_PROTO_V2, 0xff, 0xff,
        ALPS_PASS | ALPS_DUALPOINT | ALPS_PS2_INTERLEAVED } },                /* Toshiba Tecra A11-11L */
    { { 0x53, 0x02, 0x0a }, { ALPS_PROTO_V2, 0xf8, 0xf8, 0 } },
    { { 0x53, 0x02, 0x14 }, { ALPS_PROTO_V2, 0xf8, 0xf8, 0 } },
    { { 0x60, 0x03, 0xc8 }, { ALPS_PROTO_V2, 0xf8, 0xf8, 0 } },                /* HP ze1115 */
    { { 0x62, 0x02, 0x14 }, { ALPS_PROTO_V2, 0xcf, 0xcf,
        ALPS_PASS | ALPS_DUALPOINT | ALPS_PS2_INTERLEAVED } },                /* Dell Latitude E5500, E6400, E6500, Precision M4400 */
    { { 0x63, 0x02, 0x0a }, { ALPS_PROTO_V2, 0xf8, 0xf8, 0 } },
    { { 0x63, 0x02, 0x14 }, { ALPS_PROTO_V2, 0xf8, 0xf8, 0 } },
    { { 0x63, 0x02, 0x28 }, { ALPS_PROTO_V2, 0xf8, 0xf8, ALPS_FW_BK_2 } },            /* Fujitsu Siemens S6010 */
    { { 0x63, 0x02, 0x3c }, { ALPS_PROTO_V2, 0x8f, 0x8f, ALPS_WHEEL } },            /* Toshiba Satellite S2400-103 */
    { { 0x63, 0x02, 0x50 }, { ALPS_PROTO_V2, 0xef, 0xef, ALPS_FW_BK_1 } },            /* NEC Versa L320 */
    { { 0x63, 0x02, 0x64 }, { ALPS_PROTO_V2, 0xf8, 0xf8, 0 } },
    { { 0x63, 0x03, 0xc8 }, { ALPS_PROTO_V2, 0xf8, 0xf8, ALPS_PASS | ALPS_DUALPOINT } },    /* Dell Latitude D800 */
    { { 0x73, 0x00, 0x0a }, { ALPS_PROTO_V2, 0xf8, 0xf8, ALPS_DUALPOINT } },        /* ThinkPad R61 8918-5QG */
    { { 0x73, 0x00, 0x14 }, { ALPS_PROTO_V6, 0xff, 0xff, ALPS_DUALPOINT } },        /* Dell XT2 */
    { { 0x73, 0x02, 0x0a }, { ALPS_PROTO_V2, 0xf8, 0xf8, 0 } },
    { { 0x73, 0x02, 0x14 }, { ALPS_PROTO_V2, 0xf8, 0xf8, ALPS_FW_BK_2 } },            /* Ahtec Laptop */
    { { 0x73, 0x02, 0x50 }, { ALPS_PROTO_V2, 0xcf, 0xcf, ALPS_FOUR_BUTTONS } },        /* Dell Vostro 1400 */
};

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

// =============================================================================
// ALPS Class Implementation  //////////////////////////////////////////////////
// =============================================================================

OSDefineMetaClassAndStructors(ALPS, IOHIPointing);

UInt32 ALPS::deviceType()
{ return NX_EVS_DEVICE_TYPE_MOUSE; };

UInt32 ALPS::interfaceID()
{ return NX_EVS_DEVICE_INTERFACE_BUS_ACE; };

IOItemCount ALPS::buttonCount() { return _buttonCount; };
IOFixed     ALPS::resolution()  { return _resolution << 16; };

#define abs(x) ((x) < 0 ? -(x) : (x))

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

bool ALPS::init(OSDictionary *dict) {

    //
    // Initialize this object's minimal state. This is invoked right after this
    // object is instantiated.
    //

    if (!super::init(dict)) {
        return false;
    }

    // initialize state...
    for (int i = 0; i < MAX_TOUCHES; i++)
        fingerStates[i].virtualFingerIndex = -1;

    memset(freeFingerTypes, true, kMT2FingerTypeCount);
    freeFingerTypes[kMT2FingerTypeUndefined] = false;

    // announce version
    extern kmod_info_t kmod_info;
    DEBUG_LOG("ALPS: Version %s starting on OS X Darwin %d.%d.\n", kmod_info.version, version_major, version_minor);

    setProperty("Revision", 24, 32);

    return true;
}

void ALPS::injectVersionDependentProperties(OSDictionary *config) {
    // inject properties specific to the version of Darwin that is runnning...
    char buf[32];
    OSDictionary* dict = NULL;
    do
    {
        // check for "Darwin major.minor"
        snprintf(buf, sizeof(buf), "Darwin %d.%d", version_major, version_minor);
        if ((dict = OSDynamicCast(OSDictionary, config->getObject(buf))))
            break;
        // check for "Darwin major.x"
        snprintf(buf, sizeof(buf), "Darwin %d.x", version_major);
        if ((dict = OSDynamicCast(OSDictionary, config->getObject(buf))))
            break;
        // check for "Darwin 16+" (this is what is used currently, other formats are for future)
        if (version_major >= 16 && (dict = OSDynamicCast(OSDictionary, config->getObject("Darwin 16+"))))
            break;
    } while (0);

    if (dict)
    {
        // found version specific properties above, inject...
        if (OSCollectionIterator* iter = OSCollectionIterator::withCollection(dict))
        {
            // Note: OSDictionary always contains OSSymbol*
            while (const OSSymbol* key = static_cast<const OSSymbol*>(iter->getNextObject()))
            {
                if (OSObject* value = dict->getObject(key))
                    setProperty(key, value);
            }
            iter->release();
        }
    }
}

ALPS *ALPS::probe(IOService *provider, SInt32 *score) {
    DEBUG_LOG("ALPS: probe entered...\n");

    //
    // The driver has been instructed to verify the presence of the actual
    // hardware we represent. We are guaranteed by the controller that the
    // mouse clock is enabled and the mouse itself is disabled (thus it
    // won't send any asynchronous mouse data that may mess up the
    // responses expected by the commands we send it).
    //

    if (!super::probe(provider, score))
        return 0;

    _device = (ApplePS2MouseDevice *) provider;

    // find config specific to Platform Profile
    OSDictionary* list = OSDynamicCast(OSDictionary, getProperty(kPlatformProfile));
    OSDictionary* config = _device->getController()->makeConfigurationNode(list, "ALPS GlidePoint");
    if (config)
    {
        // if DisableDevice is Yes, then do not load at all...
        OSBoolean* disable = OSDynamicCast(OSBoolean, config->getObject(kDisableDevice));
        if (disable && disable->isTrue())
        {
            config->release();
            _device = 0;
            return 0;
        }
#ifdef DEBUG
        // save configuration for later/diagnostics...
        setProperty(kMergedConfiguration, config);
#endif
        // load settings specific to Platform Profile
        setParamPropertiesGated(config);
        injectVersionDependentProperties(config);
        OSSafeReleaseNULL(config);
    }

    _device->lock();
    resetMouse();

    bool success;
    if (identify() != 0) {
        success = false;
    } else {
        success = true;
        IOLog("ALPS: TouchPad driver started...\n");
    }
    _device->unlock();

    _device = 0;

    return success ? this : 0;
}

bool ALPS::resetMouse() {
    TPS2Request<3> request;

    // Reset mouse
    request.commands[0].command = kPS2C_SendCommandAndCompareAck;
    request.commands[0].inOrOut = kDP_Reset;
    request.commands[1].command = kPS2C_ReadDataPort;
    request.commands[1].inOrOut = 0;
    request.commands[2].command = kPS2C_ReadDataPort;
    request.commands[2].inOrOut = 0;
    request.commandsCount = 3;
    assert(request.commandsCount <= countof(request.commands));
    _device->submitRequestAndBlock(&request);

    // Verify the result
    if (request.commands[1].inOrOut != kSC_Reset && request.commands[2].inOrOut != kSC_ID) {
        IOLog("ALPS: Failed to reset mouse, return values did not match. [0x%02x, 0x%02x]\n", request.commands[1].inOrOut, request.commands[2].inOrOut);
        return false;
    }
    return true;
}

bool ALPS::handleOpen(IOService *forClient, IOOptionBits options, void *arg) {
    if (forClient && forClient->getProperty(VOODOO_INPUT_IDENTIFIER)) {
        voodooInputInstance = forClient;
        voodooInputInstance->retain();

        return true;
    }
    return super::handleOpen(forClient, options, arg);
}

void ALPS::handleClose(IOService *forClient, IOOptionBits options) {
    OSSafeReleaseNULL(voodooInputInstance);
    super::handleClose(forClient, options);
}

bool ALPS::start( IOService * provider ) {
    //
    // The driver has been instructed to start. This is called after a
    // successful probe and match.
    //

    if (!super::start(provider))
        return false;

    //
    // Maintain a pointer to and retain the provider object.
    //

    _device = (ApplePS2MouseDevice *) provider;
    _device->retain();

    //
    // Advertise the current state of the tapping feature.
    //
    // Must add this property to let our superclass know that it should handle
    // trackpad acceleration settings from user space.  Without this, tracking
    // speed adjustments from the mouse prefs panel have no effect.
    //

    setProperty(kIOHIDPointerAccelerationTypeKey, kIOHIDTrackpadAccelerationType);
    setProperty(kIOHIDScrollAccelerationTypeKey, kIOHIDTrackpadScrollAccelerationKey);
    setProperty(kIOHIDScrollResolutionKey, _scrollresolution << 16, 32);
    // added for Sierra precise scrolling (credit usr-sse2)
    setProperty("HIDScrollResolutionX", _scrollresolution << 16, 32);
    setProperty("HIDScrollResolutionY", _scrollresolution << 16, 32);

    //
    // Setup workloop with command gate for thread synchronization...
    //
    IOWorkLoop* pWorkLoop = getWorkLoop();
    _cmdGate = IOCommandGate::commandGate(this);
    if (!pWorkLoop || !_cmdGate)
    {
        _device->release();
        _device = nullptr;
        return false;
    }

    pWorkLoop->addEventSource(_cmdGate);

    //
    // Lock the controller during initialization
    //

    _device->lock();

    attachedHIDPointerDevices = OSSet::withCapacity(1);
    registerHIDPointerNotifications();

    //
    // Perform any implementation specific device initialization
    //
    if (!deviceSpecificInit()) {
        _device->unlock();
        _device->release();
        return false;
    }

    //
    // Install our driver's interrupt handler, for asynchronous data delivery.
    //

    _device->installInterruptAction(this,
                                    OSMemberFunctionCast(PS2InterruptAction, this, &ALPS::interruptOccurred),
                                    OSMemberFunctionCast(PS2PacketAction, this, &ALPS::packetReady));
    _interruptHandlerInstalled = true;

    // now safe to allow other threads
    _device->unlock();

    //
    // Install our power control handler.
    //

    _device->installPowerControlAction( this,
                                       OSMemberFunctionCast(PS2PowerControlAction, this, &ALPS::setDevicePowerState) );
    _powerControlHandlerInstalled = true;

    //
    // Request message registration for keyboard to trackpad communication
    //

    //setProperty(kDeliverNotifications, true);

    return true;
}


void ALPS::stop(IOService *provider) {

    DEBUG_LOG("%s: stop called\n", getName());

    //
    // The driver has been instructed to stop.  Note that we must break all
    // connections to other service objects now (ie. no registered actions,
    // no pointers and retains to objects, etc), if any.
    //

    assert(_device == provider);

    unregisterHIDPointerNotifications();
    OSSafeReleaseNULL(attachedHIDPointerDevices);

    ignoreall = false;

    //
    // Disable the mouse itself, so that it may stop reporting mouse events.
    //

    setTouchPadEnable(false);

    // free up timer for scroll momentum
    IOWorkLoop* pWorkLoop = getWorkLoop();
    if (pWorkLoop)
    {
        if (_cmdGate)
        {
            pWorkLoop->removeEventSource(_cmdGate);
            _cmdGate->release();
            _cmdGate = 0;
        }
    }

    //
    // Uninstall the interrupt handler.
    //

    if (_interruptHandlerInstalled)
    {
        _device->uninstallInterruptAction();
        _interruptHandlerInstalled = false;
    }

    //
    // Uninstall the power control handler.
    //

    if (_powerControlHandlerInstalled)
    {
        _device->uninstallPowerControlAction();
        _powerControlHandlerInstalled = false;
    }

    //
    // Release the pointer to the provider object.
    //

    OSSafeReleaseNULL(_device);

    super::stop(provider);
}

PS2InterruptResult ALPS::interruptOccurred(UInt8 data) {
    //
    // This will be invoked automatically from our device when asynchronous
    // events need to be delivered. Process the trackpad data. Do NOT issue
    // any BLOCKING commands to our device in this context.
    //

    UInt8 *packet = _ringBuffer.head();

    /* Save first packet */
    if (0 == _packetByteCount) {
        packet[0] = data;
    }

    /* Reset PSMOUSE_BAD_DATA flag */
    priv.PSMOUSE_BAD_DATA = false;

    /*
     * Check if we are dealing with a bare PS/2 packet, presumably from
     * a device connected to the external PS/2 port. Because bare PS/2
     * protocol does not have enough constant bits to self-synchronize
     * properly we only do this if the device is fully synchronized.
     * Can not distinguish V8's first byte from PS/2 packet's
     */
    if (priv.proto_version != ALPS_PROTO_V8 &&
        (packet[0] & 0xc8) == 0x08) {
        if (_packetByteCount == 3) {
            DEBUG_LOG("ALPS: Dealing with bare PS/2 packet\n");
            //dispatchRelativePointerEventWithPacket(packet, kPacketLengthSmall); //Dr Hurt: allow this?
            priv.PSMOUSE_BAD_DATA = true;
            _ringBuffer.advanceHead(priv.pktsize);
            return kPS2IR_packetReady;
        }
        packet[_packetByteCount++] = data;
        return kPS2IR_packetBuffering;
    }

    /* Check for PS/2 packet stuffed in the middle of ALPS packet. */
    if ((priv.flags & ALPS_PS2_INTERLEAVED) &&
        _packetByteCount >= 4 && (packet[3] & 0x0f) == 0x0f) {
        priv.PSMOUSE_BAD_DATA = true;
        _ringBuffer.advanceHead(priv.pktsize);
        return kPS2IR_packetReady;
    }

    /* alps_is_valid_first_byte */
    if ((packet[0] & priv.mask0) != priv.byte0) {
        priv.PSMOUSE_BAD_DATA = true;
        _ringBuffer.advanceHead(priv.pktsize);
        return kPS2IR_packetReady;
    }

    /* Bytes 2 - pktsize should have 0 in the highest bit */
    if (priv.proto_version < ALPS_PROTO_V5 &&
        _packetByteCount >= 2 && _packetByteCount <= priv.pktsize &&
        (packet[_packetByteCount - 1] & 0x80)) {
        priv.PSMOUSE_BAD_DATA = true;
        _ringBuffer.advanceHead(priv.pktsize);
        return kPS2IR_packetReady;
    }

    /* alps_is_valid_package_v7 */
    if (priv.proto_version == ALPS_PROTO_V7 &&
        (((_packetByteCount == 3) && ((packet[2] & 0x40) != 0x40)) ||
         ((_packetByteCount == 4) && ((packet[3] & 0x48) != 0x48)) ||
         ((_packetByteCount == 6) && ((packet[5] & 0x40) != 0x0)))) {
        priv.PSMOUSE_BAD_DATA = true;
        _ringBuffer.advanceHead(priv.pktsize);
        return kPS2IR_packetReady;
    }

    /* alps_is_valid_package_ss4_v2 */
    if (priv.proto_version == ALPS_PROTO_V8 &&
        ((_packetByteCount == 4 && ((packet[3] & 0x08) != 0x08)) ||
         (_packetByteCount == 6 && ((packet[5] & 0x10) != 0x0)))) {
        priv.PSMOUSE_BAD_DATA = true;
        _ringBuffer.advanceHead(priv.pktsize);
        return kPS2IR_packetReady;
    }

    packet[_packetByteCount++] = data;
    if (_packetByteCount == priv.pktsize)
    {
        _ringBuffer.advanceHead(priv.pktsize);
        return kPS2IR_packetReady;
    }
    return kPS2IR_packetBuffering;
}

void ALPS::packetReady() {
    // empty the ring buffer, dispatching each packet...
    while (_ringBuffer.count() >= priv.pktsize) {
        UInt8 *packet = _ringBuffer.tail();
        if (priv.PSMOUSE_BAD_DATA == false) {
            if (!ignoreall)
                (this->*process_packet)(packet);
        } else {
            IOLog("ALPS: an invalid or bare packet has been dropped...\n");
            /* Might need to perform a full HW reset here if we keep receiving bad packets (consecutively) */
        }
        _packetByteCount = 0;
        _ringBuffer.advanceTail(priv.pktsize);
    }
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

bool ALPS::deviceSpecificInit() {

    // Setup expected packet size
    priv.pktsize = priv.proto_version == ALPS_PROTO_V4 ? 8 : 6;

    if (!(this->*hw_init)()) {
        goto init_fail;
    }

    return true;

init_fail:
    IOLog("ALPS: Hardware initialization failed. TouchPad probably won't work\n");
    resetMouse();
    return false;
}

/* ============================================================================================== */
/* ==============================||\\ alps.c Implementation //||================================= */
/* ============================================================================================== */


void ALPS::alps_process_packet_v1_v2(UInt8 *packet) {

    // Check if input is disabled via ApplePS2Keyboard request
    if (ignoreall)
        return;

    int x, y, z, ges, fin, left, right, middle, buttons = 0, fingers = 0;
    //int back = 0, forward = 0;
    uint64_t now_abs;

    clock_get_uptime(&now_abs);

    if (priv.proto_version == ALPS_PROTO_V1) {
        left = packet[2] & 0x10;
        right = packet[2] & 0x08;
        middle = 0;
        x = packet[1] | ((packet[0] & 0x07) << 7);
        y = packet[4] | ((packet[3] & 0x07) << 7);
        z = packet[5];
    } else {
        left = packet[3] & 1;
        right = packet[3] & 2;
        middle = packet[3] & 4;
        x = packet[1] | ((packet[2] & 0x78) << (7 - 3));
        y = packet[4] | ((packet[3] & 0x70) << (7 - 4));
        z = packet[5];
    }

    // macOS does not support forward and back buttons
    /*
    if (priv.flags & ALPS_FW_BK_1) {
        back = packet[0] & 0x10;
        forward = packet[2] & 4;
    }

    if (priv.flags & ALPS_FW_BK_2) {
        back = packet[3] & 4;
        forward = packet[2] & 4;
        if ((middle = forward && back)) {
            forward = back = 0;
        }
    }
    */

    ges = packet[2] & 1;
    fin = packet[2] & 2;

    /* To make button reporting compatible with rest of driver */
    buttons |= left ? 0x01 : 0;
    buttons |= right ? 0x02 : 0;
    buttons |= middle ? 0x04 : 0;

    if ((priv.flags & ALPS_DUALPOINT) && z == 127) {
        int dx, dy;
        dx = x > 383 ? (x - 768) : x;
        dy = -(y > 255 ? (y - 512) : y);

        dispatchRelativePointerEventX(dx, dy, buttons, now_abs);
        return;
    }

    /* Some models have separate stick button bits */
    if (priv.flags & ALPS_STICK_BITS) {
        left |= packet[0] & 1;
        right |= packet[0] & 2;
        middle |= packet[0] & 4;
    }

    /* Convert hardware tap to a reasonable Z value */
    if (ges && !fin) {
        z = 40;
    }

    // REVIEW: Check if this is correct
    /*
     * A "tap and drag" operation is reported by the hardware as a transition
     * from (!fin && ges) to (fin && ges). This should be translated to the
     * sequence Z>0, Z==0, Z>0, so the Z==0 event has to be generated manually.
     */
    // if (ges && fin && !priv.prev_fin) {
    //     z = 0;
    //     fingers = 0;
    //     dispatchRelativePointerEventX(x, y, buttons, now_abs);
    // }

    priv.prev_fin = fin;

    fingers = z > 30 ? 1 : 0;

    dispatchRelativePointerEventX(x, y, buttons, now_abs);
}

static void alps_get_bitmap_points(unsigned int map,
                                   struct alps_bitmap_point *low,
                                   struct alps_bitmap_point *high,
                                   int *fingers)
{
    struct alps_bitmap_point *point;
    int i, bit, prev_bit = 0;

    point = low;
    for (i = 0; map != 0; i++, map >>= 1) {
        bit = map & 1;
        if (bit) {
            if (!prev_bit) {
                point->start_bit = i;
                point->num_bits = 0;
                (*fingers)++;
            }
            point->num_bits++;
        } else {
            if (prev_bit)
                point = high;
        }
        prev_bit = bit;
    }
}

/*
 * Process bitmap data from semi-mt protocols. Returns the number of
 * fingers detected. A return value of 0 means at least one of the
 * bitmaps was empty.
 *
 * The bitmaps don't have enough data to track fingers, so this function
 * only generates points representing a bounding box of all contacts.
 * These points are returned in fields->mt when the return value
 * is greater than 0.
 */
int ALPS::alps_process_bitmap(struct alps_data *priv,
                              struct alps_fields *fields)
{

    int i, fingers_x = 0, fingers_y = 0, fingers, closest;
    struct alps_bitmap_point x_low = {0,}, x_high = {0,};
    struct alps_bitmap_point y_low = {0,}, y_high = {0,};
    struct input_mt_pos corner[4];


    if (!fields->x_map || !fields->y_map) {
        return 0;
    }

    alps_get_bitmap_points(fields->x_map, &x_low, &x_high, &fingers_x);
    alps_get_bitmap_points(fields->y_map, &y_low, &y_high, &fingers_y);

    /*
     * Fingers can overlap, so we use the maximum count of fingers
     * on either axis as the finger count.
     */
    fingers = max(fingers_x, fingers_y);

    /*
     * If an axis reports only a single contact, we have overlapping or
     * adjacent fingers. Divide the single contact between the two points.
     */
    if (fingers_x == 1) {
        i = x_low.num_bits / 2;
        x_low.num_bits = x_low.num_bits - i;
        x_high.start_bit = x_low.start_bit + i;
        x_high.num_bits = max(i, 1);
    }

    if (fingers_y == 1) {
        i = y_low.num_bits / 2;
        y_low.num_bits = y_low.num_bits - i;
        y_high.start_bit = y_low.start_bit + i;
        y_high.num_bits = max(i, 1);
    }

    /* top-left corner */
    corner[0].x = (priv->x_max * (2 * x_low.start_bit + x_low.num_bits - 1)) /
    (2 * (priv->x_bits - 1));
    corner[0].y = (priv->y_max * (2 * y_low.start_bit + y_low.num_bits - 1)) /
    (2 * (priv->y_bits - 1));

    /* top-right corner */
    corner[1].x = (priv->x_max * (2 * x_high.start_bit + x_high.num_bits - 1)) /
    (2 * (priv->x_bits - 1));
    corner[1].y = (priv->y_max * (2 * y_low.start_bit + y_low.num_bits - 1)) /
    (2 * (priv->y_bits - 1));

    /* bottom-right corner */
    corner[2].x = (priv->x_max * (2 * x_high.start_bit + x_high.num_bits - 1)) /
    (2 * (priv->x_bits - 1));
    corner[2].y = (priv->y_max * (2 * y_high.start_bit + y_high.num_bits - 1)) /
    (2 * (priv->y_bits - 1));

    /* bottom-left corner */
    corner[3].x = (priv->x_max * (2 * x_low.start_bit + x_low.num_bits - 1)) /
    (2 * (priv->x_bits - 1));
    corner[3].y = (priv->y_max * (2 * y_high.start_bit + y_high.num_bits - 1)) /
    (2 * (priv->y_bits - 1));

    /* x-bitmap order is reversed on v5 touchpads  */
    if (priv->proto_version == ALPS_PROTO_V5) {
        for (i = 0; i < 4; i++)
            corner[i].x = priv->x_max - corner[i].x;
    }

    /* y-bitmap order is reversed on v3 and v4 touchpads  */
    if (priv->proto_version == ALPS_PROTO_V3 || priv->proto_version == ALPS_PROTO_V4) {
        for (i = 0; i < 4; i++)
            corner[i].y = priv->y_max - corner[i].y;
    }

    /*
     * We only select a corner for the second touch once per 2 finger
     * touch sequence to avoid the chosen corner (and thus the coordinates)
     * jumping around when the first touch is in the middle.
     */
    if (priv->second_touch == -1) {
        /* Find corner closest to our st coordinates */
        closest = 0x7fffffff;
        for (i = 0; i < 4; i++) {
            int dx = fields->st.x - corner[i].x;
            int dy = fields->st.y - corner[i].y;
            int distance = dx * dx + dy * dy;

            if (distance < closest) {
                priv->second_touch = i;
                closest = distance;
            }
        }
        /* And select the opposite corner to use for the 2nd touch */
        priv->second_touch = (priv->second_touch + 2) % 4;
    }

    fields->mt[0] = fields->st;
    fields->mt[1] = corner[priv->second_touch];

#if DEBUG
    IOLog("ALPS: BITMAP\n");

    unsigned int ymap = fields->y_map;

    for (int i = 0; ymap != 0; i++, ymap >>= 1) {
        unsigned int xmap = fields->x_map;
        char bitLog[160];
        strlcpy(bitLog, "ALPS: ", sizeof("ALPS: ") + 1);

        for (int j = 0; xmap != 0; j++, xmap >>= 1) {
            strcat(bitLog, (ymap & 1 && xmap & 1) ? "1 " : "0 ");
        }

        IOLog("ALPS: %s\n", bitLog);
    }

    IOLog("ALPS: Process Bitmap, Corner=%d, Fingers=%d, x1=%d, x2=%d, y1=%d, y2=%d xmap=%d ymap=%d\n", priv->second_touch, fingers, fields->mt[0].x, fields->mt[1].x, fields->mt[0].y, fields->mt[1].y, fields->x_map, fields->y_map);
#endif // DEBUG
    return fingers;
}

void ALPS::alps_process_trackstick_packet_v3(UInt8 *packet) {
    int x, y, z, left, right, middle;
    uint64_t now_abs;
    UInt32 buttons = 0, raw_buttons = 0;

    /* It should be a DualPoint when received trackstick packet */
    if (!(priv.flags & ALPS_DUALPOINT)) {
        DEBUG_LOG("ALPS: Rejected trackstick packet from non DualPoint device\n");
        return;
    }

    /* Sanity check packet */
    if (!(packet[0] & 0x40)) {
        DEBUG_LOG("ALPS: Bad trackstick packet, disregarding...\n");
        return;
    }

    /* There is a special packet that seems to indicate the end
     * of a stream of trackstick data. Filter these out
     */
    if (packet[1] == 0x7f && packet[2] == 0x7f && packet[3] == 0x7f) {
        return;
    }

    x = (SInt8) (((packet[0] & 0x20) << 2) | (packet[1] & 0x7f));
    y = (SInt8) (((packet[0] & 0x10) << 3) | (packet[2] & 0x7f));
    z = (packet[4] & 0x7f);

    /*
     * The x and y values tend to be quite large, and when used
     * alone the trackstick is difficult to use. Scale them down
     * to compensate.
     */
    x /= 8;
    y /= 8;

    /* To get proper movement direction */
    y = -y;

    clock_get_uptime(&now_abs);

    /*
     * Most ALPS models report the trackstick buttons in the touchpad
     * packets, but a few report them here. No reliable way has been
     * found to differentiate between the models upfront, so we enable
     * the quirk in response to seeing a button press in the trackstick
     * packet.
     */
    left = packet[3] & 0x01;
    right = packet[3] & 0x02;
    middle = packet[3] & 0x04;

    if (!(priv.quirks & ALPS_QUIRK_TRACKSTICK_BUTTONS) &&
        (left || middle || right)) {
        priv.quirks |= ALPS_QUIRK_TRACKSTICK_BUTTONS;
    }

    if (priv.quirks & ALPS_QUIRK_TRACKSTICK_BUTTONS) {
        raw_buttons |= left ? 0x01 : 0;
        raw_buttons |= right ? 0x02 : 0;
        raw_buttons |= middle ? 0x04 : 0;
    }

    /* Button status can appear in normal packet */
    if (0 == raw_buttons) {
        buttons = lastbuttons;
    } else {
        buttons = raw_buttons;
        lastbuttons = buttons;
    }

    /* If middle button is pressed, switch to scroll mode. Else, move pointer normally */
    if (0 == (buttons & 0x04)) {
        dispatchRelativePointerEventX(x, y, buttons, now_abs);
    } else {
        dispatchScrollWheelEventX(-y, -x, 0, now_abs);
    }
}

bool ALPS::alps_decode_buttons_v3(struct alps_fields *f, unsigned char *p) {
    f->left = !!(p[3] & 0x01);
    f->right = !!(p[3] & 0x02);
    f->middle = !!(p[3] & 0x04);

    f->ts_left = !!(p[3] & 0x10);
    f->ts_right = !!(p[3] & 0x20);
    f->ts_middle = !!(p[3] & 0x40);
    return true;
}

bool ALPS::alps_decode_pinnacle(struct alps_fields *f, UInt8 *p) {
    f->first_mp = !!(p[4] & 0x40);
    f->is_mp = !!(p[0] & 0x40);

    if (f->is_mp) {
        f->fingers = (p[5] & 0x3) + 1;
        f->x_map = ((p[4] & 0x7e) << 8) |
        ((p[1] & 0x7f) << 2) |
        ((p[0] & 0x30) >> 4);
        f->y_map = ((p[3] & 0x70) << 4) |
        ((p[2] & 0x7f) << 1) |
        (p[4] & 0x01);
    } else {
        f->st.x = ((p[1] & 0x7f) << 4) | ((p[4] & 0x30) >> 2) |
        ((p[0] & 0x30) >> 4);
        f->st.y = ((p[2] & 0x7f) << 4) | (p[4] & 0x0f);
        f->pressure = p[5] & 0x7f;

        alps_decode_buttons_v3(f, p);
    }
    return true;
}

bool ALPS::alps_decode_rushmore(struct alps_fields *f, UInt8 *p) {
    f->first_mp = !!(p[4] & 0x40);
    f->is_mp = !!(p[5] & 0x40);

    if (f->is_mp) {
        f->fingers = max((p[5] & 0x3), ((p[5] >> 2) & 0x3)) + 1;
        f->x_map = ((p[5] & 0x10) << 11) |
        ((p[4] & 0x7e) << 8) |
        ((p[1] & 0x7f) << 2) |
        ((p[0] & 0x30) >> 4);
        f->y_map = ((p[5] & 0x20) << 6) |
        ((p[3] & 0x70) << 4) |
        ((p[2] & 0x7f) << 1) |
        (p[4] & 0x01);
    } else {
        f->st.x = ((p[1] & 0x7f) << 4) | ((p[4] & 0x30) >> 2) |
        ((p[0] & 0x30) >> 4);
        f->st.y = ((p[2] & 0x7f) << 4) | (p[4] & 0x0f);
        f->pressure = p[5] & 0x7f;

        alps_decode_buttons_v3(f, p);
    }
    return true;
}

bool ALPS::alps_decode_dolphin(struct alps_fields *f, UInt8 *p) {
    uint64_t palm_data = 0;

    f->first_mp = !!(p[0] & 0x02);
    f->is_mp = !!(p[0] & 0x20);

    if (!f->is_mp) {
        f->st.x = ((p[1] & 0x7f) | ((p[4] & 0x0f) << 7));
        f->st.y = ((p[2] & 0x7f) | ((p[4] & 0xf0) << 3));
        f->pressure = (p[0] & 4) ? 0 : p[5] & 0x7f;
        alps_decode_buttons_v3(f, p);
    } else {
        f->fingers = ((p[0] & 0x6) >> 1 |
                      (p[0] & 0x10) >> 2);

        palm_data = (p[1] & 0x7f) |
        ((p[2] & 0x7f) << 7) |
        ((p[4] & 0x7f) << 14) |
        ((p[5] & 0x7f) << 21) |
        ((p[3] & 0x07) << 28) |
        (((uint64_t)p[3] & 0x70) << 27) |
        (((uint64_t)p[0] & 0x01) << 34);

        /* Y-profile is stored in P(0) to p(n-1), n = y_bits; */
        f->y_map = palm_data & (BIT(priv.y_bits) - 1);

        /* X-profile is stored in p(n) to p(n+m-1), m = x_bits; */
        f->x_map = (palm_data >> priv.y_bits) &
        (BIT(priv.x_bits) - 1);
    }
    return true;
}

void ALPS::alps_process_touchpad_packet_v3_v5(UInt8 *packet) {
    int fingers = 0, fingerCount = 0;
    struct alps_fields f;

    // Check if input is disabled via ApplePS2Keyboard request
    if (ignoreall)
        return;

    memset(&f, 0, sizeof(f));

    (this->*decode_fields)(&f, packet);
    /*
     * There's no single feature of touchpad position and bitmap packets
     * that can be used to distinguish between them. We rely on the fact
     * that a bitmap packet should always follow a position packet with
     * bit 6 of packet[4] set.
     */
    if (priv.multi_packet) {
        /*
         * Sometimes a position packet will indicate a multi-packet
         * sequence, but then what follows is another position
         * packet. Check for this, and when it happens process the
         * position packet as usual.
         */
        if (f.is_mp) {
            fingers = f.fingers;
            /*
             * Bitmap processing uses position packet's coordinate
             * data, so we need to do decode it first.
             */
            (this->*decode_fields)(&f, priv.multi_data);
            if (alps_process_bitmap(&priv, &f) == 0) {
                fingers = 0; /* Use st data */
            }
        } else {
            fingers = f.fingers;
            priv.multi_packet = 0;
        }
    }

    /*
     * Bit 6 of byte 0 is not usually set in position packets. The only
     * times it seems to be set is in situations where the data is
     * suspect anyway, e.g. a palm resting flat on the touchpad. Given
     * this combined with the fact that this bit is useful for filtering
     * out misidentified bitmap packets, we reject anything with this
     * bit set.
     */
    //if (f.is_mp) {
    //    return;
    //}

    if (!priv.multi_packet && (f.first_mp)) {
        priv.multi_packet = 1;
        memcpy(priv.multi_data, packet, sizeof(priv.multi_data));
        return;
    } else {
        priv.multi_packet = 0;
        if (fingers < 2) {
            fingers = f.pressure > 0 ? 1 : 0;
            priv.second_touch = -1;
        }
    }

    // add fingers to VoodooInput
    fingerCount = fingers;
    clampedFingerCount = fingerCount;

    DEBUG_LOG("ALPS: Amount of finger(s) accessing alps_process_touchpad_packet_v3_v5: %d\n", clampedFingerCount);

    // remove one finger as loop 'i' starts
    // w/ '0' & clampedFingerCount with '1'
    // also this will cut maximum allowed
    if (clampedFingerCount > MAX_TOUCHES)
        clampedFingerCount = MAX_TOUCHES;

    for (int i = 0; i < fingers; i++) {

        /* Use st data when we don't have mt data */
        if (priv.multi_packet == 0) {
            f.mt[i].x = f.st.x;
            f.mt[i].y = f.st.y;
        }

        // invert y moving
        f.mt[i].y = priv.y_max - f.mt[i].y;

        fingerStates[i].x = f.mt[i].x;
        fingerStates[i].y = f.mt[i].y;
        fingerStates[i].z = i;

        if (fingerStates[i].x > X_MAX_POSITIVE)
            fingerStates[i].x -= 1 << ABS_POS_BITS;
        else if (fingerStates[i].x == X_MAX_POSITIVE)
            fingerStates[i].x = XMAX;

        if (fingerStates[i].y > Y_MAX_POSITIVE)
            fingerStates[i].y -= 1 << ABS_POS_BITS;
        else if (fingerStates[i].y == Y_MAX_POSITIVE)
            fingerStates[i].y = YMAX;

        DEBUG_LOG("ALPS: fingerStates[0] report: x: %d, y: %d, z: %d\n", fingerStates[i].x, fingerStates[i].y, fingerStates[i].z);
    }

    if (renumberFingers())
        sendTouchData();

    alps_buttons(f);
}

void ALPS::alps_process_packet_v3(UInt8 *packet) {
    /*
     * v3 protocol packets come in three types, two representing
     * touchpad data and one representing trackstick data.
     * Trackstick packets seem to be distinguished by always
     * having 0x3f in the last byte. This value has never been
     * observed in the last byte of either of the other types
     * of packets.
     */
    if (packet[5] == 0x3f) {
        alps_process_trackstick_packet_v3(packet);
        return;
    }

    alps_process_touchpad_packet_v3_v5(packet);
}

void ALPS::alps_process_packet_v6(UInt8 *packet) {
    // Check if input is disabled via ApplePS2Keyboard request
    if (ignoreall)
        return;

    int x, y, z;
    // int left, right, middle;
    int buttons = 0;

    uint64_t now_abs;
    clock_get_uptime(&now_abs);

    /*
     * We can use Byte5 to distinguish if the packet is from Touchpad
     * or Trackpoint.
     * Touchpad:	0 - 0x7E
     * Trackpoint:	0x7F
     */
    if (packet[5] == 0x7F) {
        /* It should be a DualPoint when received Trackpoint packet */
        if (!(priv.flags & ALPS_DUALPOINT)) {
            DEBUG_LOG("ALPS: Rejected trackstick packet from non DualPoint device\n");
            return;
        }

        /* Trackpoint packet */
        x = packet[1] | ((packet[3] & 0x20) << 2);
        y = packet[2] | ((packet[3] & 0x40) << 1);
        z = packet[4];

        left = packet[3] & 0x01;
        right = packet[3] & 0x02;
        middle = packet[3] & 0x04;

        buttons |= left ? 0x01 : 0;
        buttons |= right ? 0x02 : 0;
        buttons |= middle ? 0x04 : 0;

        /* To prevent the cursor jump when finger lifted */
        if (x == 0x7F && y == 0x7F && z == 0x7F)
            x = y = z = 0;

        // Y is inverted
        y = -y;

        /* Divide 4 since trackpoint's speed is too fast */
        dispatchRelativePointerEventX(x / 4, y / 4, buttons, now_abs);
        return;
    }

    /* Touchpad packet */
    struct alps_fields f;

    f.mt[0].x = packet[1] | ((packet[3] & 0x78) << 4);
    f.mt[0].y = packet[2] | ((packet[4] & 0x78) << 4);
    z = packet[5];
    f.pressure = z;
    f.left = packet[3] & 0x01;
    f.right = packet[3] & 0x02;

    f.fingers = z > 30 ? 1 : 0;

    buttons |= f.left ? 0x01 : 0;
    buttons |= f.right ? 0x02 : 0;

    dispatchRelativePointerEventX(f.mt[0].x, f.mt[0].y, buttons, now_abs);
}

void ALPS::alps_process_packet_v4(UInt8 *packet) {
    // Check if input is disabled via ApplePS2Keyboard request
    if (ignoreall)
        return;

    SInt32 offset;
    // UInt32 buttons = 0;
    struct alps_fields f;

    f.fingers = 0;

    /*
     * v4 has a 6-byte encoding for bitmap data, but this data is
     * broken up between 3 normal packets. Use priv.multi_packet to
     * track our position in the bitmap packet.
     */
    if (packet[6] & 0x40) {
        /* sync, reset position */
        priv.multi_packet = 0;
    }

    if (priv.multi_packet > 2) {
        return;
    }

    offset = 2 * priv.multi_packet;
    priv.multi_data[offset] = packet[6];
    priv.multi_data[offset + 1] = packet[7];

    f.left = packet[4] & 0x01;
    f.right = packet[4] & 0x02;

    f.st.x = ((packet[1] & 0x7f) << 4) | ((packet[3] & 0x30) >> 2) |
    ((packet[0] & 0x30) >> 4);
    f.st.y = ((packet[2] & 0x7f) << 4) | (packet[3] & 0x0f);
    f.pressure = packet[5] & 0x7f;

    if (++priv.multi_packet > 2) {
        priv.multi_packet = 0;

        f.x_map = ((priv.multi_data[2] & 0x1f) << 10) |
        ((priv.multi_data[3] & 0x60) << 3) |
        ((priv.multi_data[0] & 0x3f) << 2) |
        ((priv.multi_data[1] & 0x60) >> 5);
        f.y_map = ((priv.multi_data[5] & 0x01) << 10) |
        ((priv.multi_data[3] & 0x1f) << 5) |
        (priv.multi_data[1] & 0x1f);

        f.fingers = alps_process_bitmap(&priv, &f);

    }

    /* Use st data when we don't have mt data */
    if (f.fingers < 2) {
        f.mt[0].x = f.st.x;
        f.mt[0].y = f.st.y;
        f.fingers = f.pressure > 0 ? 1 : 0;
        priv.second_touch = -1;
    }

    DEBUG_LOG("ALPS: There are currently %d finger(s) accessing alps_parse_hw_state\n", f.fingers);

    if (f.fingers >= 2) {
        fingerStates[1].x = f.mt[1].x;
        fingerStates[1].y = f.mt[1].y;
        fingerStates[1].z = f.pressure;

        if (fingerStates[1].x > X_MAX_POSITIVE)
            fingerStates[1].x -= 1 << ABS_POS_BITS;
        else if (fingerStates[1].x == X_MAX_POSITIVE)
            fingerStates[1].x = XMAX;

        if (fingerStates[1].y > Y_MAX_POSITIVE)
            fingerStates[1].y -= 1 << ABS_POS_BITS;
        else if (fingerStates[1].y == Y_MAX_POSITIVE)
            fingerStates[1].y = YMAX;
    }
    // normal "packet"
    fingerStates[0].x = f.mt[0].x;
    fingerStates[0].y = f.mt[0].y;
    fingerStates[0].z = f.pressure;

    DEBUG_LOG("ALPS: fingerStates[0] report: x: %d, y: %d, z: %d\n", fingerStates[0].x, fingerStates[0].y, fingerStates[0].z);

    if (fingerStates[0].x > X_MAX_POSITIVE)
        fingerStates[0].x -= 1 << ABS_POS_BITS;
    else if (fingerStates[0].x == X_MAX_POSITIVE)
        fingerStates[0].x = XMAX;

    if (fingerStates[0].y > Y_MAX_POSITIVE)
        fingerStates[0].y -= 1 << ABS_POS_BITS;
    else if (fingerStates[0].y == Y_MAX_POSITIVE)
        fingerStates[0].y = YMAX;

    int fingerCount = 0;
    if (fingerStates[0].z > z_finger) {
        fingerCount = 0;
        fingerCount = f.fingers;
    }

    clampedFingerCount = fingerCount;

    if (clampedFingerCount > MAX_TOUCHES)
        clampedFingerCount = MAX_TOUCHES;

    if (renumberFingers())
        sendTouchData();

    alps_buttons(f);
}

unsigned char ALPS::alps_get_packet_id_v7(UInt8 *byte) {
    unsigned char packet_id;

    if (byte[4] & 0x40)
        packet_id = V7_PACKET_ID_TWO;
    else if (byte[4] & 0x01)
        packet_id = V7_PACKET_ID_MULTI;
    else if ((byte[0] & 0x10) && !(byte[4] & 0x43))
        packet_id = V7_PACKET_ID_NEW;
    else if (byte[1] == 0x00 && byte[4] == 0x00)
        packet_id = V7_PACKET_ID_IDLE;
    else
        packet_id = V7_PACKET_ID_UNKNOWN;

    return packet_id;
}

void ALPS::alps_get_finger_coordinate_v7(struct input_mt_pos *mt,
                                         UInt8 *pkt,
                                         UInt8 pkt_id)
{
    mt[0].x = ((pkt[2] & 0x80) << 4);
    mt[0].x |= ((pkt[2] & 0x3F) << 5);
    mt[0].x |= ((pkt[3] & 0x30) >> 1);
    mt[0].x |= (pkt[3] & 0x07);
    mt[0].y = (pkt[1] << 3) | (pkt[0] & 0x07);

    mt[1].x = ((pkt[3] & 0x80) << 4);
    mt[1].x |= ((pkt[4] & 0x80) << 3);
    mt[1].x |= ((pkt[4] & 0x3F) << 4);
    mt[1].y = ((pkt[5] & 0x80) << 3);
    mt[1].y |= ((pkt[5] & 0x3F) << 4);

    switch (pkt_id) {
        case V7_PACKET_ID_TWO:
            mt[1].x &= ~0x000F;
            mt[1].y |= 0x000F;
            /* Detect false-positive touches where x & y report max value */
            if (mt[1].y == 0x7ff && mt[1].x == 0xff0)
                mt[1].x = 0;
            /* y gets set to 0 at the end of this function */
            break;

        case V7_PACKET_ID_MULTI:
            mt[1].x &= ~0x003F;
            mt[1].y &= ~0x0020;
            mt[1].y |= ((pkt[4] & 0x02) << 4);
            mt[1].y |= 0x001F;
            break;

        case V7_PACKET_ID_NEW:
            mt[1].x &= ~0x003F;
            mt[1].x |= (pkt[0] & 0x20);
            mt[1].y |= 0x000F;
            break;
    }

    mt[0].y = 0x7FF - mt[0].y;
    mt[1].y = 0x7FF - mt[1].y;
}

int ALPS::alps_get_mt_count(struct input_mt_pos *mt) {
    int i, fingers = 0;

    for (i = 0; i < MAX_TOUCHES; i++) {
        if (mt[i].x != 0 || mt[i].y != 0)
            fingers++;
    }

    return fingers;
}

bool ALPS::alps_decode_packet_v7(struct alps_fields *f, UInt8 *p){
    //IOLog("ALPS: Decode V7 touchpad Packet... 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x\n", p[0], p[1], p[2], p[3], p[4], p[5]);

    unsigned char pkt_id;

    pkt_id = alps_get_packet_id_v7(p);
    if (pkt_id == V7_PACKET_ID_IDLE) {
        DEBUG_LOG("ALPS: V7_PACKET_ID_IDLE\n");
        return true;
    }
    if (pkt_id == V7_PACKET_ID_UNKNOWN) {
        DEBUG_LOG("ALPS: V7_PACKET_ID_UNKNOWN\n");
        return false;
    }

    /*
     * NEW packets are send to indicate a discontinuity in the finger
     * coordinate reporting. Specifically a finger may have moved from
     * slot 0 to 1 or vice versa. INPUT_MT_TRACK takes care of this for
     * us.
     *
     * NEW packets have 3 problems:
     * 1) They do not contain middle / right button info (on non clickpads)
     *    this can be worked around by preserving the old button state
     * 2) They do not contain an accurate fingercount, and they are
     *    typically send when the number of fingers changes. We cannot use
     *    the old finger count as that may mismatch with the amount of
     *    touch coordinates we've available in the NEW packet
     * 3) Their x data for the second touch is inaccurate leading to
     *    a possible jump of the x coordinate by 16 units when the first
     *    non NEW packet comes in
     * Since problems 2 & 3 cannot be worked around, just ignore them.
     */
    if (pkt_id == V7_PACKET_ID_NEW) {
        DEBUG_LOG("ALPS: V7_PACKET_ID_NEW\n");
        return true;
    }

    alps_get_finger_coordinate_v7(f->mt, p, pkt_id);

    if (pkt_id == V7_PACKET_ID_TWO) {
        DEBUG_LOG("ALPS: V7_PACKET_ID_TWO\n");
        f->fingers = alps_get_mt_count(f->mt);
    }
    else { /* pkt_id == V7_PACKET_ID_MULTI */
        DEBUG_LOG("ALPS: V7_PACKET_ID_MULTI\n");
        f->fingers = 3 + (p[5] & 0x03);
    }

    f->left = (p[0] & 0x80) >> 7;
    if (priv.flags & ALPS_BUTTONPAD) {
        if (p[0] & 0x20)
            f->fingers++;
        if (p[0] & 0x10)
            f->fingers++;
    } else {
        f->right = (p[0] & 0x20) >> 5;
        f->middle = (p[0] & 0x10) >> 4;
    }

    /* Sometimes a single touch is reported in mt[1] rather then mt[0] */
    if (f->fingers == 1 && f->mt[0].x == 0 && f->mt[0].y == 0) {
        f->mt[0].x = f->mt[1].x;
        f->mt[0].y = f->mt[1].y;
        f->mt[1].x = 0;
        f->mt[1].y = 0;
    }
    return true;
}

void ALPS::alps_process_trackstick_packet_v7(UInt8 *packet) {
    int x, y, z, left, right, middle;
    int buttons = 0;

    uint64_t now_abs;
    clock_get_uptime(&now_abs);

    /* It should be a DualPoint when received trackstick packet */
    if (!(priv.flags & ALPS_DUALPOINT)) {
        IOLog("ALPS: Rejected trackstick packet from non DualPoint device\n");
        return;
    }

    x = (SInt8) ((packet[2] & 0xbf) | ((packet[3] & 0x10) << 2));
    y = (SInt8) ((packet[3] & 0x07) | (packet[4] & 0xb8) | ((packet[3] & 0x20) << 1));
    z = (packet[5] & 0x3f) | ((packet[3] & 0x80) >> 1);

    // Y is inverted
    y = -y;

    left = (packet[1] & 0x01);
    right = (packet[1] & 0x02) >> 1;
    middle = (packet[1] & 0x04) >> 2;

    buttons |= left ? 0x01 : 0;
    buttons |= right ? 0x02 : 0;
    buttons |= middle ? 0x04 : 0;

    lastTrackStickButtons = buttons;
    buttons |= lastTouchpadButtons;

    /* If middle button is pressed, switch to scroll mode. Else, move pointer normally */
    if (0 == (buttons & 0x04)) {
        dispatchRelativePointerEventX(x, y, buttons, now_abs);
    } else {
        dispatchScrollWheelEventX(-y, -x, 0, now_abs);
    }
}

void ALPS::alps_process_touchpad_packet_v7(UInt8 *packet){
    struct alps_fields f;

    // Check if input is disabled via ApplePS2Keyboard request
    if (ignoreall)
        return;

    memset(&f, 0, sizeof(alps_fields));

    (this->alps_decode_packet_v7)(&f, packet);

    /* Reverse y co-ordinates to have 0 at bottom for gestures to work */
    f.mt[0].y = priv.y_max - f.mt[0].y;
    f.mt[1].y = priv.y_max - f.mt[1].y;

    DEBUG_LOG("ALPS: Amount of finger(s) accessing alps_process_touchpad_packet_v7: %d\n", f.fingers);

    if (f.fingers >= 2) {
        fingerStates[1].x = f.mt[1].x;
        fingerStates[1].y = f.mt[1].y;
        fingerStates[1].z = f.pressure;

        if (fingerStates[1].x > X_MAX_POSITIVE)
            fingerStates[1].x -= 1 << ABS_POS_BITS;
        else if (fingerStates[1].x == X_MAX_POSITIVE)
            fingerStates[1].x = XMAX;

        if (fingerStates[1].y > Y_MAX_POSITIVE)
            fingerStates[1].y -= 1 << ABS_POS_BITS;
        else if (fingerStates[1].y == Y_MAX_POSITIVE)
            fingerStates[1].y = YMAX;
    }
    // normal "packet"
    fingerStates[0].x = f.mt[0].x;
    fingerStates[0].y = f.mt[0].y;
    fingerStates[0].z = f.pressure;

    DEBUG_LOG("ALPS: fingerStates[0] report: x: %d, y: %d, z: %d\n", fingerStates[0].x, fingerStates[0].y, fingerStates[0].z);

    if (fingerStates[0].x > X_MAX_POSITIVE)
        fingerStates[0].x -= 1 << ABS_POS_BITS;
    else if (fingerStates[0].x == X_MAX_POSITIVE)
        fingerStates[0].x = XMAX;

    if (fingerStates[0].y > Y_MAX_POSITIVE)
        fingerStates[0].y -= 1 << ABS_POS_BITS;
    else if (fingerStates[0].y == Y_MAX_POSITIVE)
        fingerStates[0].y = YMAX;

    int fingerCount = 0;
    if (fingerStates[0].z == 0) {
        fingerCount = 0;
        fingerCount = f.fingers;
    }

    clampedFingerCount = fingerCount;

    if (clampedFingerCount > MAX_TOUCHES)
        clampedFingerCount = MAX_TOUCHES;

    if (renumberFingers())
        sendTouchData();

    alps_buttons(f);
}

void ALPS::alps_process_packet_v7(UInt8 *packet){
    if (packet[0] == 0x48 && (packet[4] & 0x47) == 0x06)
        alps_process_trackstick_packet_v7(packet);
    else
        alps_process_touchpad_packet_v7(packet);
}

unsigned char ALPS::alps_get_pkt_id_ss4_v2(UInt8 *byte) {
    unsigned char pkt_id = SS4_PACKET_ID_IDLE;

    switch (byte[3] & 0x30) {
        case 0x00:
            if (SS4_IS_IDLE_V2(byte)) {
                pkt_id = SS4_PACKET_ID_IDLE;
            } else {
                pkt_id = SS4_PACKET_ID_ONE;
            }
            break;
        case 0x10:
            /* two-finger finger positions */
            pkt_id = SS4_PACKET_ID_TWO;
            break;
        case 0x20:
            /* stick pointer */
            pkt_id = SS4_PACKET_ID_STICK;
            break;
        case 0x30:
            /* third and fourth finger positions */
            pkt_id = SS4_PACKET_ID_MULTI;
            break;
    }

    return pkt_id;
}

bool ALPS::alps_decode_ss4_v2(struct alps_fields *f, UInt8 *p){

    //struct alps_data *priv;
    unsigned char pkt_id;
    unsigned int no_data_x, no_data_y;
    uint64_t now_abs;
    clock_get_uptime(&now_abs);

    pkt_id = alps_get_pkt_id_ss4_v2(p);

    /* Current packet is 1Finger coordinate packet */
    switch (pkt_id) {
        case SS4_PACKET_ID_ONE:
            DEBUG_LOG("ALPS: SS4_PACKET_ID_ONE\n");
            f->mt[0].x = SS4_1F_X_V2(p);
            f->mt[0].y = SS4_1F_Y_V2(p);
            DEBUG_LOG("ALPS: Coordinates for SS4_PACKET_ID_ONE: %dx%d\n", f->mt[0].x, f->mt[0].y);
            f->pressure = ((SS4_1F_Z_V2(p)) * 2) & 0x7f;
            /*
             * When a button is held the device will give us events
             * with x, y, and pressure of 0. This causes annoying jumps
             * if a touch is released while the button is held.
             * Handle this by claiming zero contacts.
             */
            f->fingers = f->pressure > 0 ? 1 : 0;
            f->first_mp = 0;
            f->is_mp = 0;
            break;

        case SS4_PACKET_ID_TWO:
            DEBUG_LOG("ALPS: SS4_PACKET_ID_TWO\n");
            if (priv.flags & ALPS_BUTTONPAD) {
                if (IS_SS4PLUS_DEV(priv.dev_id)) {
                    f->mt[0].x = SS4_PLUS_BTL_MF_X_V2(p, 0);
                    f->mt[1].x = SS4_PLUS_BTL_MF_X_V2(p, 1);
                } else {
                    f->mt[0].x = SS4_BTL_MF_X_V2(p, 0);
                    f->mt[1].x = SS4_BTL_MF_X_V2(p, 1);
                }
                f->mt[0].y = SS4_BTL_MF_Y_V2(p, 0);
                f->mt[1].y = SS4_BTL_MF_Y_V2(p, 1);
            } else {
                if (IS_SS4PLUS_DEV(priv.dev_id)) {
                    f->mt[0].x = SS4_PLUS_STD_MF_X_V2(p, 0);
                    f->mt[1].x = SS4_PLUS_STD_MF_X_V2(p, 1);
                } else {
                    f->mt[0].x = SS4_STD_MF_X_V2(p, 0);
                    f->mt[1].x = SS4_STD_MF_X_V2(p, 1);
                }
                f->mt[0].y = SS4_STD_MF_Y_V2(p, 0);
                f->mt[1].y = SS4_STD_MF_Y_V2(p, 1);
            }
            DEBUG_LOG("ALPS: Coordinates for SS4_PACKET_ID_TWO: [0]:%dx%d [1]:%dx%d\n", f->mt[0].x, f->mt[0].y, f->mt[1].x, f->mt[1].y);
            f->pressure = SS4_MF_Z_V2(p, 0) ? 0x30 : 0;

            if (SS4_IS_MF_CONTINUE(p)) {
                f->first_mp = 1;
            } else {
                f->fingers = 2;
                f->first_mp = 0;
            }
            f->is_mp = 0;

            break;

        case SS4_PACKET_ID_MULTI:
            DEBUG_LOG("ALPS: SS4_PACKET_ID_MULTI\n");
            if (priv.flags & ALPS_BUTTONPAD) {
                if (IS_SS4PLUS_DEV(priv.dev_id)) {
                    f->mt[2].x = SS4_PLUS_BTL_MF_X_V2(p, 0);
                    f->mt[3].x = SS4_PLUS_BTL_MF_X_V2(p, 1);
                    no_data_x = SS4_PLUS_MFPACKET_NO_AX_BL;
                } else {
                    f->mt[2].x = SS4_BTL_MF_X_V2(p, 0);
                    f->mt[3].x = SS4_BTL_MF_X_V2(p, 1);
                    no_data_x = SS4_MFPACKET_NO_AX_BL;
                }
                no_data_y = SS4_MFPACKET_NO_AY_BL;

                f->mt[2].y = SS4_BTL_MF_Y_V2(p, 0);
                f->mt[3].y = SS4_BTL_MF_Y_V2(p, 1);
            } else {
                if (IS_SS4PLUS_DEV(priv.dev_id)) {
                    f->mt[2].x = SS4_PLUS_STD_MF_X_V2(p, 0);
                    f->mt[3].x = SS4_PLUS_STD_MF_X_V2(p, 1);
                    no_data_x = SS4_PLUS_MFPACKET_NO_AX;
                } else {
                    f->mt[2].x = SS4_STD_MF_X_V2(p, 0);
                    f->mt[3].x = SS4_STD_MF_X_V2(p, 1);
                    no_data_x = SS4_MFPACKET_NO_AX;
                }
                no_data_y = SS4_MFPACKET_NO_AY;

                f->mt[2].y = SS4_STD_MF_Y_V2(p, 0);
                f->mt[3].y = SS4_STD_MF_Y_V2(p, 1);
            }
            DEBUG_LOG("ALPS: Coordinates for SS4_PACKET_ID_MULTI: [2]:%dx%d [3]:%dx%d\n", f->mt[2].x, f->mt[2].y, f->mt[3].x, f->mt[3].y);

            f->first_mp = 0;
            f->is_mp = 1;

            if (SS4_IS_5F_DETECTED(p)) {
                f->fingers = 5;
            } else if (f->mt[3].x == no_data_x &&
                       f->mt[3].y == no_data_y) {
                f->mt[3].x = 0;
                f->mt[3].y = 0;
                f->fingers = 3;
            } else {
                f->fingers = 4;
            }
            break;

        case SS4_PACKET_ID_STICK:
            DEBUG_LOG("ALPS: SS4_PACKET_ID_STICK\n");
            /*
             * x, y, and pressure are decoded in
             * alps_process_packet_ss4_v2()
             */
            f->first_mp = 0;
            f->is_mp = 0;
            break;

        case SS4_PACKET_ID_IDLE:
        default:
            memset(f, 0, sizeof(struct alps_fields));
            break;
    }

    /* handle buttons */
    if (pkt_id == SS4_PACKET_ID_STICK) {
        f->ts_left = !!(SS4_BTN_V2(p) & 0x01);
        f->ts_right = !!(SS4_BTN_V2(p) & 0x02);
        f->ts_middle = !!(SS4_BTN_V2(p) & 0x04);
    } else {
        f->left = !!(SS4_BTN_V2(p) & 0x01);
        if (!(priv.flags & ALPS_BUTTONPAD)) {
            f->right = !!(SS4_BTN_V2(p) & 0x02);
            f->middle = !!(SS4_BTN_V2(p) & 0x04);
        }
    }
    return true;
}

void ALPS::alps_process_packet_ss4_v2(UInt8 *packet) {
    // Check if input is disabled via ApplePS2Keyboard request
    if (ignoreall)
        return;

    int buttons = 0;
    struct alps_fields f;
    int x, y, pressure;

    uint64_t now_abs;
    clock_get_uptime(&now_abs);

    memset(&f, 0, sizeof(struct alps_fields));
    (this->*decode_fields)(&f, packet);
    if (priv.multi_packet) {
        /*
         * Sometimes the first packet will indicate a multi-packet
         * sequence, but sometimes the next multi-packet would not
         * come. Check for  this, and when it happens process the
         * position packet as usual.
         */
        if (f.is_mp) {
            /* Now process the 1st packet */
            (this->*decode_fields)(&f, priv.multi_data);
        } else {
            priv.multi_packet = 0;
        }
    }

    /*
     * "f.is_mp" would always be '0' after merging the 1st and 2nd packet.
     * When it is set, it means 2nd packet comes without 1st packet come.
     */
    if (f.is_mp) {
        return;
    }

    /* Save the first packet */
    if (!priv.multi_packet && f.first_mp) {
        priv.multi_packet = 1;
        memcpy(priv.multi_data, packet, sizeof(priv.multi_data));
        return;
    }

    priv.multi_packet = 0;

    /* Report trackstick */
    if (alps_get_pkt_id_ss4_v2(packet) == SS4_PACKET_ID_STICK) {
        if (!(priv.flags & ALPS_DUALPOINT)) {
            IOLog("ALPS: Rejected trackstick packet from non DualPoint device\n");
            return;
        }

        x = (SInt8) (((packet[0] & 1) << 7) | (packet[1] & 0x7f));
        y = (SInt8) (((packet[3] & 1) << 7) | (packet[2] & 0x7f));
        pressure = (packet[4] & 0x7f);

        buttons |= f.ts_left ? 0x01 : 0;
        buttons |= f.ts_right ? 0x02 : 0;
        buttons |= f.ts_middle ? 0x04 : 0;

        if ((abs(x) >= 0x7f) || (abs(y) >= 0x7f)) {
            return;
        }

        // Y is inverted
        y = -y;

        // Divide by 3 since trackpoint's speed is too fast
        x /= 3;
        y /= 3;

        DEBUG_LOG("ALPS: Trackstick report: X=%d, Y=%d, Z=%d\n", x, y, pressure);
        /* If middle button is pressed, switch to scroll mode. Else, move pointer normally */
        if (0 == (buttons & 0x04)) {
            dispatchRelativePointerEventX(x, y, buttons, now_abs);
        } else {
            dispatchScrollWheelEventX(-y, -x, 0, now_abs);
        }
        return;
    }

    /* Reverse y co-ordinates to have 0 at bottom for gestures to work */
    f.mt[0].y = priv.y_max - f.mt[0].y;
    f.mt[1].y = priv.y_max - f.mt[1].y;

    DEBUG_LOG("ALPS: There are currently %d fingers in alps_process_packet_ss4_v2\n", f.fingers);

    if (f.fingers >= 2) {
        fingerStates[1].x = f.mt[1].x;
        fingerStates[1].y = f.mt[1].y;
        fingerStates[1].z = f.pressure;

        if (fingerStates[1].x > X_MAX_POSITIVE)
            fingerStates[1].x -= 1 << ABS_POS_BITS;
        else if (fingerStates[1].x == X_MAX_POSITIVE)
            fingerStates[1].x = XMAX;

        if (fingerStates[1].y > Y_MAX_POSITIVE)
            fingerStates[1].y -= 1 << ABS_POS_BITS;
        else if (fingerStates[1].y == Y_MAX_POSITIVE)
            fingerStates[1].y = YMAX;

        DEBUG_LOG("ALPS: fingerStates[1] report: x: %d, y: %d, z: %d\n", fingerStates[1].x, fingerStates[1].y, fingerStates[1].z);
    }
    // normal "packet"
    fingerStates[0].x = f.mt[0].x;
    fingerStates[0].y = f.mt[0].y;
    fingerStates[0].z = f.pressure;

    DEBUG_LOG("ALPS: fingerStates[0] report: x: %d, y: %d, z: %d\n", fingerStates[0].x, fingerStates[0].y, fingerStates[0].z);

    if (fingerStates[0].x > X_MAX_POSITIVE)
        fingerStates[0].x -= 1 << ABS_POS_BITS;
    else if (fingerStates[0].x == X_MAX_POSITIVE)
        fingerStates[0].x = XMAX;

    if (fingerStates[0].y > Y_MAX_POSITIVE)
        fingerStates[0].y -= 1 << ABS_POS_BITS;
    else if (fingerStates[0].y == Y_MAX_POSITIVE)
        fingerStates[0].y = YMAX;

    int fingerCount = 0;
    if (fingerStates[0].z > z_finger) {
        fingerCount = 0;
        fingerCount = f.fingers;
    }

    clampedFingerCount = fingerCount;

    if (clampedFingerCount > MAX_TOUCHES)
        clampedFingerCount = MAX_TOUCHES;

    if (renumberFingers())
        sendTouchData();

    alps_buttons(f);
}

bool ALPS::alps_command_mode_send_nibble(int nibble) {
    SInt32 command;
    // The largest amount of requests we will have is 2 right now
    // 1 for the initial command, and 1 for sending data OR 1 for receiving data
    // If the nibble commands at the top change then this will need to change as
    // well. For now we will just validate that the request will not overload
    // this object.
    TPS2Request<2> request;
    int cmdCount = 0, send = 0, receive = 0, i;

    if (nibble > 0xf) {
        IOLog("%s::alps_command_mode_send_nibble ERROR: nibble value is greater than 0xf, command may fail\n", getName());
    }

    request.commands[cmdCount].command = kPS2C_SendCommandAndCompareAck;
    command = priv.nibble_commands[nibble].command;
    request.commands[cmdCount++].inOrOut = command & 0xff;

    send = (command >> 12 & 0xf);
    receive = (command >> 8 & 0xf);

    if ((send > 1) || ((send + receive + 1) > 2)) {
        return false;
    }

    if (send > 0) {
        request.commands[cmdCount].command = kPS2C_SendCommandAndCompareAck;
        request.commands[cmdCount++].inOrOut = priv.nibble_commands[nibble].data;
    }

    for (i = 0; i < receive; i++) {
        request.commands[cmdCount].command = kPS2C_ReadDataPort;
        request.commands[cmdCount++].inOrOut = 0;
    }

    request.commandsCount = cmdCount;
    assert(request.commandsCount <= countof(request.commands));

    _device->submitRequestAndBlock(&request);

    return request.commandsCount == cmdCount;
}

bool ALPS::alps_command_mode_set_addr(int addr) {

    TPS2Request<1> request;
    int i, nibble;

    // DEBUG_LOG("ALPS: command mode set addr with addr command: 0x%02x\n", priv.addr_command);
    request.commands[0].command = kPS2C_SendCommandAndCompareAck;
    request.commands[0].inOrOut = priv.addr_command;
    request.commandsCount = 1;
    _device->submitRequestAndBlock(&request);

    if (request.commandsCount != 1) {
        return false;
    }

    for (i = 12; i >= 0; i -= 4) {
        nibble = (addr >> i) & 0xf;
        if (!alps_command_mode_send_nibble(nibble)) {
            return false;
        }
    }

    return true;
}

int ALPS::alps_command_mode_read_reg(int addr) {
    TPS2Request<4> request;
    ALPSStatus_t status;

    if (!alps_command_mode_set_addr(addr)) {
        DEBUG_LOG("ALPS: Failed to set addr to read register\n");
        return -1;
    }

    request.commands[0].command = kPS2C_SendCommandAndCompareAck;
    request.commands[0].inOrOut = kDP_GetMouseInformation; //sync..
    request.commands[1].command = kPS2C_ReadDataPort;
    request.commands[1].inOrOut = 0;
    request.commands[2].command = kPS2C_ReadDataPort;
    request.commands[2].inOrOut = 0;
    request.commands[3].command = kPS2C_ReadDataPort;
    request.commands[3].inOrOut = 0;
    request.commandsCount = 4;
    assert(request.commandsCount <= countof(request.commands));
    _device->submitRequestAndBlock(&request);

    if (request.commandsCount != 4) {
        return -1;
    }

    status.bytes[0] = request.commands[1].inOrOut;
    status.bytes[1] = request.commands[2].inOrOut;
    status.bytes[2] = request.commands[3].inOrOut;

    // IOLog("ALPS: read reg result: { 0x%02x, 0x%02x, 0x%02x }\n", status.bytes[0], status.bytes[1], status.bytes[2]);

    /* The address being read is returned in the first 2 bytes
     * of the result. Check that the address matches the expected
     * address.
     */
    if (addr != ((status.bytes[0] << 8) | status.bytes[1])) {
        DEBUG_LOG("ALPS: ERROR: read wrong registry value, expected: %x\n", addr);
        return -1;
    }

    return status.bytes[2];
}

bool ALPS::alps_command_mode_write_reg(int addr, UInt8 value) {

    if (!alps_command_mode_set_addr(addr)) {
        return false;
    }

    return alps_command_mode_write_reg(value);
}

bool ALPS::alps_command_mode_write_reg(UInt8 value) {
    if (!alps_command_mode_send_nibble((value >> 4) & 0xf)) {
        return false;
    }
    if (!alps_command_mode_send_nibble(value & 0xf)) {
        return false;
    }

    return true;
}

bool ALPS::alps_rpt_cmd(SInt32 init_command, SInt32 init_arg, SInt32 repeated_command, ALPSStatus_t *report) {
    TPS2Request<9> request;
    int byte0, cmd;
    cmd = byte0 = 0;

    if (init_command) {
        request.commands[cmd].command = kPS2C_SendCommandAndCompareAck;
        request.commands[cmd++].inOrOut = kDP_SetMouseResolution;
        request.commands[cmd].command = kPS2C_SendCommandAndCompareAck;
        request.commands[cmd++].inOrOut = init_arg;
    }


    // 3X run command
    request.commands[cmd].command = kPS2C_SendCommandAndCompareAck;
    request.commands[cmd++].inOrOut = repeated_command;
    request.commands[cmd].command = kPS2C_SendCommandAndCompareAck;
    request.commands[cmd++].inOrOut = repeated_command;
    request.commands[cmd].command = kPS2C_SendCommandAndCompareAck;
    request.commands[cmd++].inOrOut = repeated_command;

    // Get info/result
    request.commands[cmd].command = kPS2C_SendCommandAndCompareAck;
    request.commands[cmd++].inOrOut = kDP_GetMouseInformation;
    byte0 = cmd;
    request.commands[cmd].command = kPS2C_ReadDataPort;
    request.commands[cmd++].inOrOut = 0;
    request.commands[cmd].command = kPS2C_ReadDataPort;
    request.commands[cmd++].inOrOut = 0;
    request.commands[cmd].command = kPS2C_ReadDataPort;
    request.commands[cmd++].inOrOut = 0;
    request.commandsCount = cmd;
    assert(request.commandsCount <= countof(request.commands));
    _device->submitRequestAndBlock(&request);

    report->bytes[0] = request.commands[byte0].inOrOut;
    report->bytes[1] = request.commands[byte0+1].inOrOut;
    report->bytes[2] = request.commands[byte0+2].inOrOut;

    DEBUG_LOG("ALPS: %02x report: [0x%02x 0x%02x 0x%02x]\n",
              repeated_command,
              report->bytes[0],
              report->bytes[1],
              report->bytes[2]);

    return request.commandsCount == cmd;
}

bool ALPS::alps_enter_command_mode() {
    DEBUG_LOG("ALPS: enter command mode\n");
    TPS2Request<4> request;
    ALPSStatus_t status;

    if (!alps_rpt_cmd(NULL, NULL, kDP_MouseResetWrap, &status)) {
        IOLog("ALPS: Failed to enter command mode!\n");
        return false;
    }
    return true;
}

bool ALPS::alps_exit_command_mode() {
    DEBUG_LOG("ALPS: exit command mode\n");
    TPS2Request<1> request;

    request.commands[0].command = kPS2C_SendCommandAndCompareAck;
    request.commands[0].inOrOut = kDP_SetMouseStreamMode;
    request.commandsCount = 1;
    assert(request.commandsCount <= countof(request.commands));
    _device->submitRequestAndBlock(&request);

    return true;
}

/*
 * For DualPoint devices select the device that should respond to
 * subsequent commands. It looks like glidepad is behind stickpointer,
 * I'd thought it would be other way around...
 */
bool ALPS::alps_passthrough_mode_v2(bool enable) {
    int cmd = enable ? kDP_SetMouseScaling2To1 : kDP_SetMouseScaling1To1;
    TPS2Request<4> request;

    request.commands[0].command = kPS2C_SendCommandAndCompareAck;
    request.commands[0].inOrOut = cmd;
    request.commands[1].command = kPS2C_SendCommandAndCompareAck;
    request.commands[1].inOrOut = cmd;
    request.commands[2].command = kPS2C_SendCommandAndCompareAck;
    request.commands[2].inOrOut = cmd;
    request.commands[3].command = kPS2C_SendCommandAndCompareAck;
    request.commands[3].inOrOut = kDP_SetDefaultsAndDisable;
    request.commandsCount = 4;
    assert(request.commandsCount <= countof(request.commands));
    _device->submitRequestAndBlock(&request);

    return request.commandsCount == 4;
}

bool ALPS::alps_absolute_mode_v1_v2() {

    /* Try ALPS magic knock - 4 disable before enable */
    ps2_command_short(kDP_SetDefaultsAndDisable);
    ps2_command_short(kDP_SetDefaultsAndDisable);
    ps2_command_short(kDP_SetDefaultsAndDisable);
    ps2_command_short(kDP_SetDefaultsAndDisable);
    ps2_command_short(kDP_Enable);

    /*
     * Switch mouse to poll (remote) mode so motion data will not
     * get in our way
     */
    ps2_command_short(kDP_MouseSetPoll);

    return true;
}

int ALPS::alps_monitor_mode_send_word(int word) {
    int i, nibble;

    for (i = 0; i <= 8; i += 4) {
        nibble = (word >> i) & 0xf;
        alps_command_mode_send_nibble(nibble);
    }

    return 0;
}

int ALPS::alps_monitor_mode_write_reg(int addr, int value) {
    ps2_command_short(kDP_Enable);
    alps_monitor_mode_send_word(0x0A0);
    alps_monitor_mode_send_word(addr);
    alps_monitor_mode_send_word(value);
    ps2_command_short(kDP_SetDefaultsAndDisable);

    return 0;
}

int ALPS::alps_monitor_mode(bool enable) {
    TPS2Request<4> request;
    int cmd = 0;

    if (enable) {
        /* EC E9 F5 F5 E7 E6 E7 E9 to enter monitor mode */
        ps2_command_short(kDP_MouseResetWrap);
        request.commands[cmd].command = kPS2C_SendCommandAndCompareAck;
        request.commands[cmd++].inOrOut = kDP_GetMouseInformation;
        request.commands[cmd].command = kPS2C_ReadDataPort;
        request.commands[cmd++].inOrOut = 0;
        request.commands[cmd].command = kPS2C_ReadDataPort;
        request.commands[cmd++].inOrOut = 0;
        request.commands[cmd].command = kPS2C_ReadDataPort;
        request.commands[cmd++].inOrOut = 0;
        request.commandsCount = cmd;
        assert(request.commandsCount <= countof(request.commands));
        _device->submitRequestAndBlock(&request);

        ps2_command_short(kDP_SetDefaultsAndDisable);
        ps2_command_short(kDP_SetDefaultsAndDisable);
        ps2_command_short(kDP_SetMouseScaling2To1);
        ps2_command_short(kDP_SetMouseScaling1To1);
        ps2_command_short(kDP_SetMouseScaling2To1);

        /* Get Info */
        request.commands[cmd].command = kPS2C_SendCommandAndCompareAck;
        request.commands[cmd++].inOrOut = kDP_GetMouseInformation;
        request.commands[cmd].command = kPS2C_ReadDataPort;
        request.commands[cmd++].inOrOut = 0;
        request.commands[cmd].command = kPS2C_ReadDataPort;
        request.commands[cmd++].inOrOut = 0;
        request.commands[cmd].command = kPS2C_ReadDataPort;
        request.commands[cmd++].inOrOut = 0;
        request.commandsCount = cmd;
        assert(request.commandsCount <= countof(request.commands));
        _device->submitRequestAndBlock(&request);
    } else {
        /* EC to exit monitor mode */
        ps2_command_short(kDP_MouseResetWrap);
    }

    return 0;
}

void ALPS::alps_absolute_mode_v6() {
    // enter monitor mode, to write the register /
    alps_monitor_mode(true);
    alps_monitor_mode_write_reg(0x000, 0x181);
    alps_monitor_mode(false);
}

bool ALPS::alps_get_status(ALPSStatus_t *status) {
    /* Get status: 0xF5 0xF5 0xF5 0xE9 */
    return alps_rpt_cmd(NULL, NULL, kDP_SetDefaultsAndDisable, status);
}

/*
 * Turn touchpad tapping on or off. The sequences are:
 * 0xE9 0xF5 0xF5 0xF3 0x0A to enable,
 * 0xE9 0xF5 0xF5 0xE8 0x00 to disable.
 * My guess that 0xE9 (GetInfo) is here as a sync point.
 * For models that also have stickpointer (DualPoints) its tapping
 * is controlled separately (0xE6 0xE6 0xE6 0xF3 0x14|0x0A) but
 * we don't fiddle with it.
 */
bool ALPS::alps_tap_mode(bool enable) {
    int cmd = enable ? kDP_SetMouseSampleRate : kDP_SetMouseResolution;
    UInt8 tapArg = enable ? 0x0A : 0x00;
    TPS2Request<8> request;
    ALPSStatus_t result;

    request.commands[0].command = kPS2C_SendCommandAndCompareAck;
    request.commands[0].inOrOut = kDP_GetMouseInformation;
    request.commands[1].command = kPS2C_ReadDataPort;
    request.commands[1].inOrOut = 0;
    request.commands[2].command = kPS2C_ReadDataPort;
    request.commands[2].inOrOut = 0;
    request.commands[3].command = kPS2C_ReadDataPort;
    request.commands[3].inOrOut = 0;
    request.commands[4].command = kPS2C_SendCommandAndCompareAck;
    request.commands[4].inOrOut = kDP_SetDefaultsAndDisable;
    request.commands[5].command = kPS2C_SendCommandAndCompareAck;
    request.commands[5].inOrOut = kDP_SetDefaultsAndDisable;
    request.commands[6].command = kPS2C_SendCommandAndCompareAck;
    request.commands[6].inOrOut = cmd;
    request.commands[7].command = kPS2C_SendCommandAndCompareAck;
    request.commands[7].inOrOut = tapArg;
    request.commandsCount = 8;
    _device->submitRequestAndBlock(&request);

    if (request.commandsCount != 8) {
        DEBUG_LOG("ALPS: Enabling tap mode failed before getStatus call, command count=%d\n",
                  request.commandsCount);
        return false;
    }

    return alps_get_status(&result);
}


bool ALPS::alps_hw_init_v1_v2() {
    TPS2Request<1> request;

    if (priv.flags & ALPS_PASS) {
        if (!alps_passthrough_mode_v2(true)) {
            return false;
        }
    }

    if (!alps_tap_mode(true)) {
        IOLog("ALPS: Failed to enable hardware tapping\n");
        return false;
    }

    if (!alps_absolute_mode_v1_v2()) {
        IOLog("ALPS: Failed to enable absolute mode\n");
        return false;
    }

    if (priv.flags & ALPS_PASS) {
        if (!alps_passthrough_mode_v2(false)) {
            return false;
        }
    }

    /* ALPS needs stream mode, otherwise it won't report any data */
    ps2_command_short(kDP_SetMouseStreamMode);

    return true;
}

bool ALPS::alps_hw_init_v6() {
    /* Enter passthrough mode to let trackpoint enter 6byte raw mode */
    alps_passthrough_mode_v2(true);

    /* alps_trackstick_enter_extended_mode_v3_v6 */
    ps2_command_short(kDP_SetMouseScaling1To1);
    ps2_command_short(kDP_SetMouseScaling1To1);
    ps2_command_short(kDP_SetMouseScaling1To1);
    ps2_command(0xC8, kDP_SetMouseSampleRate);
    ps2_command(0x14, kDP_SetMouseSampleRate);

    alps_passthrough_mode_v2(false);

    alps_absolute_mode_v6();

    return true;
}

/*
 * Enable or disable passthrough mode to the trackstick.
 */
bool ALPS::alps_passthrough_mode_v3(int regBase, bool enable) {
    int regVal;
    bool ret = false;

    DEBUG_LOG("ALPS: passthrough mode enable=%d\n", enable);

    if (!alps_enter_command_mode()) {
        IOLog("ALPS: Failed to enter command mode while enabling passthrough mode\n");
        return false;
    }

    regVal = alps_command_mode_read_reg(regBase + 0x0008);
    if (regVal == -1) {
        IOLog("ALPS: Failed to read register while setting up passthrough mode\n");
        goto error;
    }

    if (enable) {
        regVal |= 0x01;
    } else {
        regVal &= ~0x01;
    }

    ret = alps_command_mode_write_reg(regVal);

error:
    if (!alps_exit_command_mode()) {
        IOLog("ALPS: failed to exit command mode while enabling passthrough mode v3\n");
        return false;
    }

    return ret;
}

/* Must be in command mode when calling this function */
bool ALPS::alps_absolute_mode_v3() {

    int regVal;

    regVal = alps_command_mode_read_reg(0x0004);
    if (regVal == -1) {
        return false;
    }

    regVal |= 0x06;
    if (!alps_command_mode_write_reg(regVal)) {
        return false;
    }

    return true;
}

IOReturn ALPS::alps_probe_trackstick_v3_v7(int regBase) {
    int ret = kIOReturnIOError, regVal;

    if (!alps_enter_command_mode()) {
        goto error;
    }

    regVal = alps_command_mode_read_reg(regBase + 0x08);

    if (regVal == -1) {
        goto error;
    }

    /* bit 7: trackstick is present */
    ret = regVal & 0x80 ? 0 : kIOReturnNoDevice;

error:
    alps_exit_command_mode();
    return ret;
}

IOReturn ALPS::alps_setup_trackstick_v3(int regBase) {
    IOReturn ret = 0;
    ALPSStatus_t report;
    TPS2Request<3> request;

    /*
     * We need to configure trackstick to report data for touchpad in
     * extended format. And also we need to tell touchpad to expect data
     * from trackstick in extended format. Without this configuration
     * trackstick packets sent from touchpad are in basic format which is
     * different from what we expect.
     */
    if (!alps_passthrough_mode_v3(regBase, true)) {
        return kIOReturnIOError;
    }

    /*
     * E7 report for the trackstick
     *
     * There have been reports of failures to seem to trace back
     * to the above trackstick check failing. When these occur
     * this E7 report fails, so when that happens we continue
     * with the assumption that there isn't a trackstick after
     * all.
     */
    if (!alps_rpt_cmd(NULL, NULL, kDP_SetMouseScaling2To1, &report)) {
        IOLog("ALPS: Failed to initialize trackstick (E7 report failed)\n");
        ret = kIOReturnNoDevice;
    } else {
        /*
         * Not sure what this does, but it is absolutely
         * essential. Without it, the touchpad does not
         * work at all and the trackstick just emits normal
         * PS/2 packets.
         */
        request.commands[0].command = kPS2C_SendCommandAndCompareAck;
        request.commands[0].inOrOut = kDP_SetMouseScaling1To1;
        request.commands[1].command = kPS2C_SendCommandAndCompareAck;
        request.commands[1].inOrOut = kDP_SetMouseScaling1To1;
        request.commands[2].command = kPS2C_SendCommandAndCompareAck;
        request.commands[2].inOrOut = kDP_SetMouseScaling1To1;
        request.commandsCount = 3;
        assert(request.commandsCount <= countof(request.commands));
        _device->submitRequestAndBlock(&request);
        if (request.commandsCount != 3) {
            IOLog("ALPS: error sending magic E6 scaling sequence\n");
            ret = kIOReturnIOError;
            goto error;
        }
        if (!(alps_command_mode_send_nibble(0x9) && alps_command_mode_send_nibble(0x4))) {
            IOLog("ALPS: Error sending magic E6 nibble sequence\n");
            ret = kIOReturnIOError;
            goto error;
        }
        DEBUG_LOG("ALPS: Sent magic E6 sequence\n");

        /*
         * This ensures the trackstick packets are in the format
         * supported by this driver. If bit 1 isn't set the packet
         * format is different.
         */
        if (!(alps_enter_command_mode() &&
              alps_command_mode_write_reg(regBase + 0x0008, 0x82) &&
              alps_exit_command_mode())) {
            ret = -kIOReturnIOError;
            //goto error;
        }
    }
error:
    if (!alps_passthrough_mode_v3(regBase, false)) {
        ret = kIOReturnIOError;
    }

    return ret;
}

bool ALPS::alps_hw_init_v3() {
    int regVal;

    if ((priv.flags & ALPS_DUALPOINT) &&
        alps_setup_trackstick_v3(ALPS_REG_BASE_PINNACLE) == kIOReturnIOError)
        goto error;

    if (!(alps_enter_command_mode() &&
          alps_absolute_mode_v3())) {
        IOLog("ALPS: Failed to enter absolute mode\n");
        goto error;
    }

    regVal = alps_command_mode_read_reg(0x0006);
    if (regVal == -1)
        goto error;
    if (!alps_command_mode_write_reg(regVal | 0x01))
        goto error;

    regVal = alps_command_mode_read_reg(0x0007);
    if (regVal == -1)
        goto error;
    if (!alps_command_mode_write_reg(regVal | 0x01))
        goto error;

    if (alps_command_mode_read_reg(0x0144) == -1)
        goto error;
    if (!alps_command_mode_write_reg(0x04))
        goto error;

    if (alps_command_mode_read_reg(0x0159) == -1)
        goto error;
    if (!alps_command_mode_write_reg(0x03))
        goto error;

    if (alps_command_mode_read_reg(0x0163) == -1)
        goto error;
    if (!alps_command_mode_write_reg(0x0163, 0x03))
        goto error;

    if (alps_command_mode_read_reg(0x0162) == -1)
        goto error;
    if (!alps_command_mode_write_reg(0x0162, 0x04))
        goto error;

    alps_exit_command_mode();

    /* Set rate and enable data reporting */
    ps2_command(0x28, kDP_SetMouseSampleRate);
    ps2_command_short(kDP_Enable);

    return true;

error:
    /*
     * Leaving the touchpad in command mode will essentially render
     * it unusable until the machine reboots, so exit it here just
     * to be safe
     */
    alps_exit_command_mode();
    return false;
}

bool ALPS::alps_get_v3_v7_resolution(int reg_pitch) {
    int reg, x_pitch, y_pitch, x_electrode, y_electrode, x_phys, y_phys;

    reg = alps_command_mode_read_reg(reg_pitch);
    if (reg < 0)
        return reg;

    x_pitch = (char)(reg << 4) >> 4; /* sign extend lower 4 bits */
    x_pitch = 50 + 2 * x_pitch; /* In 0.1 mm units */

    y_pitch = (char)reg >> 4; /* sign extend upper 4 bits */
    y_pitch = 36 + 2 * y_pitch; /* In 0.1 mm units */

    reg = alps_command_mode_read_reg(reg_pitch + 1);
    if (reg < 0)
        return reg;

    x_electrode = (char)(reg << 4) >> 4; /* sign extend lower 4 bits */
    x_electrode = 17 + x_electrode;

    y_electrode = (char)reg >> 4; /* sign extend upper 4 bits */
    y_electrode = 13 + y_electrode;

    x_phys = x_pitch * (x_electrode - 1); /* In 0.1 mm units */
    y_phys = y_pitch * (y_electrode - 1); /* In 0.1 mm units */

    priv.x_res = priv.x_max * 10 / x_phys; /* units / mm */
    priv.y_res = priv.y_max * 10 / y_phys; /* units / mm */

    /*IOLog("pitch %dx%d num-electrodes %dx%d physical size %dx%d mm res %dx%d\n",
     x_pitch, y_pitch, x_electrode, y_electrode,
     x_phys / 10, y_phys / 10, priv.x_res, priv.y_res);*/

    return true;
}

bool ALPS::alps_hw_init_rushmore_v3() {
    int regVal;

    if (priv.flags & ALPS_DUALPOINT) {
        regVal = alps_setup_trackstick_v3(ALPS_REG_BASE_RUSHMORE);
        if (regVal == kIOReturnIOError) {
            goto error;
        }
    }

    if (!alps_enter_command_mode() ||
        alps_command_mode_read_reg(0xc2d9) == -1 ||
        !alps_command_mode_write_reg(0xc2cb, 0x00)) {
        goto error;
    }

    if (!alps_get_v3_v7_resolution(0xc2da))
        goto error;

    regVal = alps_command_mode_read_reg(0xc2c6);
    if (regVal == -1)
        goto error;
    if (!alps_command_mode_write_reg(regVal & 0xfd))
        goto error;

    if (!alps_command_mode_write_reg(0xc2c9, 0x64))
        goto error;

    /* enter absolute mode */
    regVal = alps_command_mode_read_reg(0xc2c4);
    if (regVal == -1)
        goto error;
    if (!alps_command_mode_write_reg(regVal | 0x02))
        goto error;

    alps_exit_command_mode();

    /* Enable data reporting */
    ps2_command_short(kDP_Enable);

    return true;

error:
    alps_exit_command_mode();
    return false;
}

/* Must be in command mode when calling this function */
bool ALPS::alps_absolute_mode_v4() {
    int regVal;

    regVal = alps_command_mode_read_reg(0x0004);
    if (regVal == -1) {
        return false;
    }

    regVal |= 0x02;
    if (!alps_command_mode_write_reg(regVal)) {
        return false;
    }

    return true;
}

bool ALPS::alps_hw_init_v4() {

    if (!alps_enter_command_mode())
        goto error;

    if (!alps_absolute_mode_v4()) {
        IOLog("ALPS: Failed to enter absolute mode\n");
        goto error;
    }

    if (!alps_command_mode_write_reg(0x0007, 0x8c))
        goto error;

    if (!alps_command_mode_write_reg(0x0149, 0x03))
        goto error;

    if (!alps_command_mode_write_reg(0x0160, 0x03))
        goto error;

    if (!alps_command_mode_write_reg(0x017f, 0x15))
        goto error;

    if (!alps_command_mode_write_reg(0x0151, 0x01))
        goto error;

    if (!alps_command_mode_write_reg(0x0168, 0x03))
        goto error;

    if (!alps_command_mode_write_reg(0x014a, 0x03))
        goto error;

    if (!alps_command_mode_write_reg(0x0161, 0x03))
        goto error;

    alps_exit_command_mode();

    /*
     * This sequence changes the output from a 9-byte to an
     * 8-byte format. All the same data seems to be present,
     * just in a more compact format.
     */
    ps2_command(0xc8, kDP_SetMouseSampleRate);
    ps2_command(0x64, kDP_SetMouseSampleRate);
    ps2_command(0x50, kDP_SetMouseSampleRate);
    ps2_command_short(kDP_GetId);

    /* Set rate and enable data reporting */
    ps2_command(0x64, kDP_SetMouseSampleRate);
    ps2_command_short(kDP_Enable);
    return true;

error:
    /*
     * Leaving the touchpad in command mode will essentially render
     * it unusable until the machine reboots, so exit it here just
     * to be safe
     */
    alps_exit_command_mode();
    return false;
}

void ALPS::alps_get_otp_values_ss4_v2(unsigned char index, unsigned char otp[]) {
    TPS2Request<4> request;

    switch (index) {
        case 0:
            ps2_command_short(kDP_SetMouseStreamMode);
            ps2_command_short(kDP_SetMouseStreamMode);

            request.commands[0].command = kPS2C_SendCommandAndCompareAck;
            request.commands[0].inOrOut = kDP_GetMouseInformation;
            request.commands[1].command = kPS2C_ReadDataPort;
            request.commands[1].inOrOut = 0;
            request.commands[2].command = kPS2C_ReadDataPort;
            request.commands[2].inOrOut = 0;
            request.commands[3].command = kPS2C_ReadDataPort;
            request.commands[3].inOrOut = 0;
            request.commandsCount = 4;
            assert(request.commandsCount <= countof(request.commands));
            _device->submitRequestAndBlock(&request);

            otp[0] = request.commands[1].inOrOut;
            otp[1] = request.commands[2].inOrOut;
            otp[2] = request.commands[3].inOrOut;

            break;

        case 1:
            ps2_command_short(kDP_MouseSetPoll);
            ps2_command_short(kDP_MouseSetPoll);

            request.commands[0].command = kPS2C_SendCommandAndCompareAck;
            request.commands[0].inOrOut = kDP_GetMouseInformation;
            request.commands[1].command = kPS2C_ReadDataPort;
            request.commands[1].inOrOut = 0;
            request.commands[2].command = kPS2C_ReadDataPort;
            request.commands[2].inOrOut = 0;
            request.commands[3].command = kPS2C_ReadDataPort;
            request.commands[3].inOrOut = 0;
            request.commandsCount = 4;
            assert(request.commandsCount <= countof(request.commands));
            _device->submitRequestAndBlock(&request);

            otp[0] = request.commands[1].inOrOut;
            otp[1] = request.commands[2].inOrOut;
            otp[2] = request.commands[3].inOrOut;

            break;
    }
}

void ALPS::alps_update_device_area_ss4_v2(unsigned char otp[][4], struct alps_data *priv) {
    int num_x_electrode;
    int num_y_electrode;
    int x_pitch, y_pitch, x_phys, y_phys;

    DEBUG_LOG("ALPS: Accessing 'Update Device Area'\n");

    if (IS_SS4PLUS_DEV(priv->dev_id)) {
        DEBUG_LOG("ALPS: Device is SS4_PLUS\n");
        num_x_electrode = SS4PLUS_NUMSENSOR_XOFFSET + (otp[0][2] & 0x0F);
        num_y_electrode = SS4PLUS_NUMSENSOR_YOFFSET + ((otp[0][2] >> 4) & 0x0F);

        priv->x_max = (num_x_electrode - 1) * SS4PLUS_COUNT_PER_ELECTRODE;
        priv->y_max = (num_y_electrode - 1) * SS4PLUS_COUNT_PER_ELECTRODE;

        x_pitch = (otp[0][1] & 0x0F) + SS4PLUS_MIN_PITCH_MM;
        y_pitch = ((otp[0][1] >> 4) & 0x0F) + SS4PLUS_MIN_PITCH_MM;

    } else {
        DEBUG_LOG("ALPS: Device is SS4\n");
        num_x_electrode = SS4_NUMSENSOR_XOFFSET + (otp[1][0] & 0x0F);
        num_y_electrode = SS4_NUMSENSOR_YOFFSET + ((otp[1][0] >> 4) & 0x0F);

        priv->x_max = (num_x_electrode - 1) * SS4_COUNT_PER_ELECTRODE;
        priv->y_max = (num_y_electrode - 1) * SS4_COUNT_PER_ELECTRODE;

        x_pitch = ((otp[1][2] >> 2) & 0x07) + SS4_MIN_PITCH_MM;
        y_pitch = ((otp[1][2] >> 5) & 0x07) + SS4_MIN_PITCH_MM;
    }

    x_phys = x_pitch * (num_x_electrode - 1); /* In 0.1 mm units */
    y_phys = y_pitch * (num_y_electrode - 1); /* In 0.1 mm units */

    DEBUG_LOG("ALPS: Your dimensions are: %dx%d\n", x_phys, y_phys);

    priv->x_res = priv->x_max * 10 / x_phys; /* units / mm */
    priv->y_res = priv->y_max * 10 / y_phys; /* units / mm */
}

void ALPS::alps_update_btn_info_ss4_v2(unsigned char otp[][4], struct alps_data *priv) {
    unsigned char is_btnless;

    if (IS_SS4PLUS_DEV(priv->dev_id))
        is_btnless = (otp[1][0] >> 1) & 0x01;
    else
        is_btnless = (otp[1][1] >> 3) & 0x01;

    if (is_btnless)
        priv->flags |= ALPS_BUTTONPAD;

    is_btnless ? setProperty("Clickpad", kOSBooleanTrue) : setProperty("Clickpad", kOSBooleanFalse);
}

void ALPS::alps_update_dual_info_ss4_v2(unsigned char otp[][4], struct alps_data *priv) {
    bool is_dual = false;
    int reg_val = 0;

    if (IS_SS4PLUS_DEV(priv->dev_id)) {
        is_dual = (otp[0][0] >> 4) & 0x01;

        if (!is_dual) {
            /* For support TrackStick of Thinkpad L/E series */
            if (alps_exit_command_mode())
                if (alps_enter_command_mode())
                    reg_val = alps_command_mode_read_reg(0xD7);
            alps_exit_command_mode();
            ps2_command_short(kDP_Enable);

            if (reg_val == 0x0C || reg_val == 0x1D)
                is_dual = true;
        }
    }

    if (is_dual)
        priv->flags |= ALPS_DUALPOINT |
        ALPS_DUALPOINT_WITH_PRESSURE;

    is_dual ? setProperty("Trackpoint", kOSBooleanTrue) : setProperty("Trackpoint", kOSBooleanFalse);
}

void ALPS::alps_set_defaults_ss4_v2(struct alps_data *priv) {
    unsigned char otp[2][4];

    memset(otp, 0, sizeof(otp));

    alps_get_otp_values_ss4_v2(1, &otp[1][0]);
    alps_get_otp_values_ss4_v2(0, &otp[0][0]);

    alps_update_device_area_ss4_v2(otp, priv);

    alps_update_btn_info_ss4_v2(otp, priv);

    alps_update_dual_info_ss4_v2(otp, priv);
}

int ALPS::alps_dolphin_get_device_area(struct alps_data *priv) {
    int num_x_electrode, num_y_electrode;
    TPS2Request<4> request;
    int cmd = 0;
    ALPSStatus_t status;

    alps_enter_command_mode();

    ps2_command_short(kDP_MouseResetWrap);
    ps2_command_short(kDP_MouseSetPoll);
    ps2_command_short(kDP_MouseSetPoll);
    ps2_command(0x0a, kDP_SetMouseSampleRate);
    ps2_command(0x0a, kDP_SetMouseSampleRate);

    request.commands[cmd].command = kPS2C_SendCommandAndCompareAck;
    request.commands[cmd++].inOrOut = kDP_GetMouseInformation;
    request.commands[cmd].command = kPS2C_ReadDataPort;
    request.commands[cmd++].inOrOut = 0;
    request.commands[cmd].command = kPS2C_ReadDataPort;
    request.commands[cmd++].inOrOut = 0;
    request.commands[cmd].command = kPS2C_ReadDataPort;
    request.commands[cmd++].inOrOut = 0;
    request.commandsCount = cmd;
    assert(request.commandsCount <= countof(request.commands));
    _device->submitRequestAndBlock(&request);

    /* results */
    status.bytes[0] = request.commands[1].inOrOut;
    status.bytes[1] = request.commands[2].inOrOut;
    status.bytes[2] = request.commands[3].inOrOut;

    /*
     * Dolphin's sensor line number is not fixed. It can be calculated
     * by adding the device's register value with DOLPHIN_PROFILE_X/YOFFSET.
     * Further more, we can get device's x_max and y_max by multiplying
     * sensor line number with DOLPHIN_COUNT_PER_ELECTRODE.
     *
     * e.g. When we get register's sensor_x = 11 & sensor_y = 8,
     *    real sensor line number X = 11 + 8 = 19, and
     *    real sensor line number Y = 8 + 1 = 9.
     *    So, x_max = (19 - 1) * 64 = 1152, and
     *        y_max = (9 - 1) * 64 = 512.
     */
    num_x_electrode = DOLPHIN_PROFILE_XOFFSET + (status.bytes[2] & 0x0F);
    num_y_electrode = DOLPHIN_PROFILE_YOFFSET + ((status.bytes[2] >> 4) & 0x0F);
    priv->x_bits = num_x_electrode;
    priv->y_bits = num_y_electrode;
    priv->x_max = (num_x_electrode - 1) * DOLPHIN_COUNT_PER_ELECTRODE;
    priv->y_max = (num_y_electrode - 1) * DOLPHIN_COUNT_PER_ELECTRODE;

    alps_exit_command_mode();

    return 0;
}

bool ALPS::alps_hw_init_dolphin_v1() {

    ps2_command_short(kDP_SetMouseStreamMode);
    ps2_command(0x64, kDP_SetMouseSampleRate);
    ps2_command(0x28, kDP_SetMouseSampleRate);
    ps2_command_short(kDP_Enable);

    return true;
}

bool ALPS::alps_hw_init_v7(){
    int reg_val;

    if (!alps_enter_command_mode())
        goto error;

    if (alps_command_mode_read_reg(0xc2d9) == -1)
        goto error;

    if (!alps_get_v3_v7_resolution(0xc397))
        goto error;

    if (!alps_command_mode_write_reg(0xc2c9, 0x64))
        goto error;

    reg_val = alps_command_mode_read_reg(0xc2c4);
    if (reg_val == -1)
        goto error;

    if (!alps_command_mode_write_reg(reg_val | 0x02))
        goto error;

    alps_exit_command_mode();

    ps2_command(0x28, kDP_SetMouseSampleRate);
    ps2_command_short(kDP_Enable);

    return true;

error:
    alps_exit_command_mode();
    return false;
}

bool ALPS::alps_hw_init_ss4_v2() {
    /* enter absolute mode */
    ps2_command_short(kDP_SetMouseStreamMode);
    ps2_command_short(kDP_SetMouseStreamMode);
    ps2_command(0x64, kDP_SetMouseSampleRate);
    ps2_command(0x28, kDP_SetMouseSampleRate);

    /* T.B.D. Decread noise packet number, delete in the future */
    alps_exit_command_mode();
    alps_enter_command_mode();
    alps_command_mode_write_reg(0x001D, 0x20);
    alps_exit_command_mode();

    /* final init */
    ps2_command_short(kDP_Enable);

    return true;

}

void ALPS::set_protocol() {
    setProperty("ALPS Version", priv.proto_version, 32);

    priv.byte0 = 0x8f;
    priv.mask0 = 0x8f;
    priv.flags = ALPS_DUALPOINT;

    priv.x_max = 2000;
    priv.y_max = 1400;
    priv.x_bits = 15;
    priv.y_bits = 11;

    switch (priv.proto_version) {
        case ALPS_PROTO_V1:
        case ALPS_PROTO_V2:
            hw_init = &ALPS::alps_hw_init_v1_v2;
            process_packet = &ALPS::alps_process_packet_v1_v2;
            //set_abs_params = alps_set_abs_params_st;
            priv.x_max = 1023;
            priv.y_max = 767;
            break;

        case ALPS_PROTO_V3:
            hw_init = &ALPS::alps_hw_init_v3;
            process_packet = &ALPS::alps_process_packet_v3;
            //set_abs_params = alps_set_abs_params_semi_mt;
            decode_fields = &ALPS::alps_decode_pinnacle;
            priv.nibble_commands = alps_v3_nibble_commands;
            priv.addr_command = kDP_MouseResetWrap;

            if (alps_probe_trackstick_v3_v7(ALPS_REG_BASE_PINNACLE)) {
                priv.flags &= ~ALPS_DUALPOINT;
            } else {
                IOLog("ALPS: TrackStick detected...\n");
            }

            break;

        case ALPS_PROTO_V3_RUSHMORE:
            hw_init = &ALPS::alps_hw_init_rushmore_v3;
            process_packet = &ALPS::alps_process_packet_v3;
            //set_abs_params = alps_set_abs_params_semi_mt;
            decode_fields = &ALPS::alps_decode_rushmore;
            priv.nibble_commands = alps_v3_nibble_commands;
            priv.addr_command = kDP_MouseResetWrap;
            priv.x_bits = 16;
            priv.y_bits = 12;

            if (alps_probe_trackstick_v3_v7(ALPS_REG_BASE_RUSHMORE)) {
                priv.flags &= ~ALPS_DUALPOINT;
            } else {
                IOLog("ALPS: TrackStick detected...\n");
            }

            break;

        case ALPS_PROTO_V4:
            hw_init = &ALPS::alps_hw_init_v4;
            process_packet = &ALPS::alps_process_packet_v4;
            //set_abs_params = alps_set_abs_params_semi_mt;
            priv.nibble_commands = alps_v4_nibble_commands;
            priv.addr_command = kDP_SetDefaultsAndDisable;
            break;

        case ALPS_PROTO_V5:
            hw_init = &ALPS::alps_hw_init_dolphin_v1;
            process_packet = &ALPS::alps_process_touchpad_packet_v3_v5;
            decode_fields = &ALPS::alps_decode_dolphin;
            //set_abs_params = alps_set_abs_params_semi_mt;
            priv.nibble_commands = alps_v3_nibble_commands;
            priv.addr_command = kDP_MouseResetWrap;
            priv.byte0 = 0xc8;
            priv.mask0 = 0xc8;
            priv.flags = 0;
            //priv.x_max = 1360;
            //priv.y_max = 660;
            priv.x_bits = 23;
            priv.y_bits = 12;

            alps_dolphin_get_device_area(&priv);
            break;

        case ALPS_PROTO_V6:
            hw_init = &ALPS::alps_hw_init_v6;
            process_packet = &ALPS::alps_process_packet_v6;
            //set_abs_params = alps_set_abs_params_st;
            priv.nibble_commands = alps_v6_nibble_commands;
            priv.addr_command = kDP_MouseResetWrap;
            priv.byte0 = 0xc8;
            priv.mask0 = 0xc8;
            priv.flags = 0;
            priv.x_max = 2047;
            priv.y_max = 1535;
            break;

        case ALPS_PROTO_V7:
            hw_init = &ALPS::alps_hw_init_v7;
            process_packet = &ALPS::alps_process_packet_v7;
            decode_fields = &ALPS::alps_decode_packet_v7;
            //set_abs_params = alps_set_abs_params_v7;
            priv.nibble_commands = alps_v3_nibble_commands;
            priv.addr_command = kDP_MouseResetWrap;
            priv.byte0 = 0x48;
            priv.mask0 = 0x48;

            priv.x_max = 0xfff;
            priv.y_max = 0x7ff;

            if (priv.fw_ver[1] != 0xba){
                priv.flags |= ALPS_BUTTONPAD;
                IOLog("ALPS: ButtonPad Detected...\n");
            }

            if (alps_probe_trackstick_v3_v7(ALPS_REG_BASE_V7)){
                priv.flags &= ~ALPS_DUALPOINT;
            } else {
                IOLog("ALPS: TrackStick detected...\n");
            }

            break;

        case ALPS_PROTO_V8:
            hw_init = &ALPS::alps_hw_init_ss4_v2;
            process_packet = &ALPS::alps_process_packet_ss4_v2;
            decode_fields = &ALPS::alps_decode_ss4_v2;
            //set_abs_params = alps_set_abs_params_ss4_v2;
            priv.nibble_commands = alps_v3_nibble_commands;
            priv.addr_command = kDP_MouseResetWrap;
            priv.byte0 = 0x18;
            priv.mask0 = 0x18;
            priv.flags = 0;

            alps_set_defaults_ss4_v2(&priv);
            break;
    }

    if (priv.proto_version != ALPS_PROTO_V1 ||
        priv.proto_version != ALPS_PROTO_V2 ||
        priv.proto_version != ALPS_PROTO_V6)
        set_resolution();
}

bool ALPS::matchTable(ALPSStatus_t *e7, ALPSStatus_t *ec) {
    const struct alps_model_info *model;
    int i;

    IOLog("ALPS: Touchpad with Signature { %d, %d, %d }\n", e7->bytes[0], e7->bytes[1], e7->bytes[2]);

    for (i = 0; i < ARRAY_SIZE(alps_model_data); i++) {
        model = &alps_model_data[i];

        if (!memcmp(e7->bytes, model->signature, sizeof(model->signature))) {

            priv.proto_version = model->protocol_info.version;

            // log model version:
            if (priv.proto_version == ALPS_PROTO_V1) {
                IOLog("ALPS: Found an ALPS V1 TouchPad\n");
            } else if (priv.proto_version == ALPS_PROTO_V2) {
                IOLog("ALPS: Found an ALPS V2 TouchPad\n");
            } else if (priv.proto_version == ALPS_PROTO_V3_RUSHMORE) {
                IOLog("ALPS: Found an ALPS V3 Rushmore TouchPad\n");
            } else if (priv.proto_version == ALPS_PROTO_V4) {
                IOLog("ALPS: Found an ALPS V4 TouchPad\n");
            }else if (priv.proto_version == ALPS_PROTO_V6) {
                IOLog("ALPS: Found an ALPS V6 TouchPad\n");
            }

            set_protocol();

            priv.flags = model->protocol_info.flags;
            priv.byte0 = model->protocol_info.byte0;
            priv.mask0 = model->protocol_info.mask0;

            return true;
        }
    }

    return false;
}

IOReturn ALPS::identify() {
    ALPSStatus_t e6, e7, ec;

    /*
     * First try "E6 report".
     * ALPS should return 0,0,10 or 0,0,100 if no buttons are pressed.
     * The bits 0-2 of the first byte will be 1s if some buttons are
     * pressed.
     */

    if (!alps_rpt_cmd(kDP_SetMouseResolution, NULL, kDP_SetMouseScaling1To1, &e6)) {
        IOLog("ALPS: identify: not an ALPS device. Error getting E6 report\n");
        //return kIOReturnIOError;
    }

    if ((e6.bytes[0] & 0xf8) != 0 || e6.bytes[1] != 0 || (e6.bytes[2] != 10 && e6.bytes[2] != 100)) {
        IOLog("ALPS: identify: not an ALPS device. Invalid E6 report\n");
        //return kIOReturnInvalid;
    }

    /*
     * Now get the "E7" and "EC" reports.  These will uniquely identify
     * most ALPS touchpads.
     */
    if (!(alps_rpt_cmd(kDP_SetMouseResolution, NULL, kDP_SetMouseScaling2To1, &e7) &&
          alps_rpt_cmd(kDP_SetMouseResolution, NULL, kDP_MouseResetWrap, &ec) &&
          alps_exit_command_mode())) {
        IOLog("ALPS: identify: not an ALPS device. Error getting E7/EC report\n");
        return kIOReturnIOError;
    }

    if (matchTable(&e7, &ec)) {
        return 0;

    } else if (e7.bytes[0] == 0x73 && e7.bytes[1] == 0x02 && e7.bytes[2] == 0x64 &&
               ec.bytes[2] == 0x8a) {
        priv.proto_version = ALPS_PROTO_V4;
        IOLog("ALPS: Found a V4 TouchPad with ID: E7=0x%02x 0x%02x 0x%02x, EC=0x%02x 0x%02x 0x%02x\n", e7.bytes[0], e7.bytes[1], e7.bytes[2], ec.bytes[0], ec.bytes[1], ec.bytes[2]);
    } else if (e7.bytes[0] == 0x73 && e7.bytes[1] == 0x03 && e7.bytes[2] == 0x50 &&
               ec.bytes[0] == 0x73 && (ec.bytes[1] == 0x01 || ec.bytes[1] == 0x02)) {
        priv.proto_version = ALPS_PROTO_V5;
        IOLog("ALPS: Found a V5 Dolphin TouchPad with ID: E7=0x%02x 0x%02x 0x%02x, EC=0x%02x 0x%02x 0x%02x\n", e7.bytes[0], e7.bytes[1], e7.bytes[2], ec.bytes[0], ec.bytes[1], ec.bytes[2]);

    } else if (ec.bytes[0] == 0x88 &&
               ((ec.bytes[1] & 0xf0) == 0xb0 || (ec.bytes[1] & 0xf0) == 0xc0)) {
        priv.proto_version = ALPS_PROTO_V7;
        IOLog("ALPS: Found a V7 TouchPad with ID: E7=0x%02x 0x%02x 0x%02x, EC=0x%02x 0x%02x 0x%02x\n", e7.bytes[0], e7.bytes[1], e7.bytes[2], ec.bytes[0], ec.bytes[1], ec.bytes[2]);

    } else if (ec.bytes[0] == 0x88 && ec.bytes[1] == 0x08) {
        priv.proto_version = ALPS_PROTO_V3_RUSHMORE;
        IOLog("ALPS: Found a V3 Rushmore TouchPad with ID: E7=0x%02x 0x%02x 0x%02x, EC=0x%02x 0x%02x 0x%02x\n", e7.bytes[0], e7.bytes[1], e7.bytes[2], ec.bytes[0], ec.bytes[1], ec.bytes[2]);

    } else if (ec.bytes[0] == 0x88 && ec.bytes[1] == 0x07 &&
               ec.bytes[2] >= 0x90 && ec.bytes[2] <= 0x9d) {
        priv.proto_version = ALPS_PROTO_V3;
        IOLog("ALPS: Found a V3 Pinnacle TouchPad with ID: E7=0x%02x 0x%02x 0x%02x, EC=0x%02x 0x%02x 0x%02x\n", e7.bytes[0], e7.bytes[1], e7.bytes[2], ec.bytes[0], ec.bytes[1], ec.bytes[2]);

    } else if (e7.bytes[0] == 0x73 && e7.bytes[1] == 0x03 &&
               (e7.bytes[2] == 0x14 || e7.bytes[2] == 0x28)) {
        priv.proto_version = ALPS_PROTO_V8;
        IOLog("ALPS: Found a V8 TouchPad with ID: E7=0x%02x 0x%02x 0x%02x, EC=0x%02x 0x%02x 0x%02x\n", e7.bytes[0], e7.bytes[1], e7.bytes[2], ec.bytes[0], ec.bytes[1], ec.bytes[2]);

    } else if (e7.bytes[0] == 0x73 && e7.bytes[1] == 0x03 && e7.bytes[2] == 0xc8) {
        priv.proto_version = ALPS_PROTO_V9;
        IOLog("ALPS: Found a unsupported V9 TouchPad with ID: E7=0x%02x 0x%02x 0x%02x, EC=0x%02x 0x%02x 0x%02x\n", e7.bytes[0], e7.bytes[1], e7.bytes[2], ec.bytes[0], ec.bytes[1], ec.bytes[2]);

    } else {
        IOLog("ALPS: Touchpad didn't match any known IDs: E7=0x%02x 0x%02x 0x%02x, EC=0x%02x 0x%02x 0x%02x ... driver will now exit\n",
              e7.bytes[0], e7.bytes[1], e7.bytes[2], ec.bytes[0], ec.bytes[1], ec.bytes[2]);
        return kIOReturnInvalid;
    }

    /* Save Device ID and Firmware version */
    memcpy(priv.dev_id, e7.bytes, 3);
    memcpy(priv.fw_ver, ec.bytes, 3);
    set_protocol();
    return 0;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

void ALPS::setTouchPadEnable(bool enable) {
    DEBUG_LOG("ALPS: setTouchpadEnable enter\n");
    //
    // Instructs the trackpad to start or stop the reporting of data packets.
    // It is safe to issue this request from the interrupt/completion context.
    //

    if (enable) {
        initTouchPad();
    } else {
        // to disable just reset the mouse
        resetMouse();
    }
}

void ALPS::ps2_command(unsigned char value, UInt8 command) {
    TPS2Request<2> request;
    int cmdCount = 0;

    request.commands[cmdCount].command = kPS2C_SendCommandAndCompareAck;
    request.commands[cmdCount++].inOrOut = command;
    request.commands[cmdCount].command = kPS2C_SendCommandAndCompareAck;
    request.commands[cmdCount++].inOrOut = value;
    request.commandsCount = cmdCount;
    assert(request.commandsCount <= countof(request.commands));
    _device->submitRequestAndBlock(&request);

    //return request.commandsCount = cmdCount;
}

void ALPS::ps2_command_short(UInt8 command) {
    TPS2Request<1> request;
    int cmdCount = 0;

    request.commands[cmdCount].command = kPS2C_SendCommandAndCompareAck;
    request.commands[cmdCount++].inOrOut = command;
    request.commandsCount = cmdCount;
    assert(request.commandsCount <= countof(request.commands));
    _device->submitRequestAndBlock(&request);

    //return request.commandsCount = cmdCount;
}

/* ============================================================================================== */
/* ===========================||\\PROCESS AND DISPATCH TO macOS//||============================== */
/* ============================================================================================== */


void ALPS::set_resolution() {
    physical_max_x = priv.x_max * 4; // this number was determined experimentally
    physical_max_y = priv.y_max * 4.5; // this number was determined experimentally

    logical_max_x = priv.x_max;
    logical_max_y = priv.y_max;

    setProperty("X Max", priv.x_max, 32);
    setProperty("Y Max", priv.y_max, 32);

    if (priv.proto_version == ALPS_PROTO_V7) {
        margin_size_x = 3 * xupmm;

        if (maxXOverride != -1)
            logical_max_x = maxXOverride;
    }

    setProperty(VOODOO_INPUT_LOGICAL_MAX_X_KEY, logical_max_x - margin_size_x, 32);
    setProperty(VOODOO_INPUT_LOGICAL_MAX_Y_KEY, logical_max_y, 32);

    setProperty(VOODOO_INPUT_PHYSICAL_MAX_X_KEY, physical_max_x, 32);
    setProperty(VOODOO_INPUT_PHYSICAL_MAX_Y_KEY, physical_max_y, 32);

    setProperty(VOODOO_INPUT_TRANSFORM_KEY, 0ull, 32);
    setProperty("VoodooInputSupported", kOSBooleanTrue);

    registerService();

    DEBUG_LOG("VoodooPS2Trackpad: logical_max %dx%d physical_max %dx%d upmm %dx%d\n",
              logical_max_x, logical_max_y,
              physical_max_x, physical_max_y,
              xupmm, yupmm);
}

void ALPS::alps_buttons(struct alps_fields &f) {
    bool prev_left = left;
    bool prev_right = right;
    bool prev_middle = middle;
    bool prev_left_ts = left_ts;

    left = f.left;
    right = f.right | f.ts_right;
    middle = f.middle;
    left_ts = f.ts_left;

    AbsoluteTime timestamp;
    clock_get_uptime(&timestamp);
    // Physical left button (for non-Clickpads)
    // Only used if trackpad is not a clickpad
    if (!(priv.flags & ALPS_BUTTONPAD)) {
        if (left && !prev_left)
            dispatchRelativePointerEvent(0, 0, 0x01, timestamp);
        else if (prev_left && !left)
            dispatchRelativePointerEvent(0, 0, 0x00, timestamp);
    }
    // Physical right button
    if (right && !prev_right)
        dispatchRelativePointerEvent(0, 0, 0x02, timestamp);
    else if (prev_right && !right)
        dispatchRelativePointerEvent(0, 0, 0x00, timestamp);
    // Physical middle button
    if (middle && !prev_middle)
        dispatchRelativePointerEvent(0, 0, 0x04, timestamp);
    else if (prev_middle && !middle)
        dispatchRelativePointerEvent(0, 0, 0x00, timestamp);
    // Physical left button (Trackstick)
    if (left_ts && !prev_left_ts)
        dispatchRelativePointerEvent(0, 0, 0x01, timestamp);
    else if (prev_left_ts && !left_ts)
        dispatchRelativePointerEvent(0, 0, 0x00, timestamp);
}

// port from VoodooPS2SynapticsTouchpad.cpp; huge credits to @usr-sse2

#define sqr(x) ((x) * (x))
int ALPS::dist(int physicalFinger, int virtualFinger) {
    const auto &phy = fingerStates[physicalFinger];
    const auto &virt = virtualFingerStates[virtualFinger];
    return sqr(phy.x - virt.x_avg.newest()) + sqr(phy.y - virt.y_avg.newest());
}

void ALPS::assignVirtualFinger(int physicalFinger) {
    if (physicalFinger < 0 || physicalFinger >= MAX_TOUCHES) {
        IOLog("alps_parse_hw_state: ALPS::assignVirtualFinger ERROR: invalid physical finger %d\n", physicalFinger);
        return;
    }
    for (int j = 0; j < MAX_TOUCHES; j++) {
        virtual_finger_state &vfj = virtualFingerStates[j];
        if (!vfj.touch) {
            fingerStates[physicalFinger].virtualFingerIndex = j;
            vfj.touch = true;
            vfj.x_avg.reset();
            vfj.y_avg.reset();
            assignFingerType(vfj);
            break;
        }
    }
}

void ALPS::assignFingerType(virtual_finger_state &vf) {
    vf.fingerType = kMT2FingerTypeUndefined;
    for (MT2FingerType i = kMT2FingerTypeIndexFinger; i < kMT2FingerTypeCount; i = (MT2FingerType)(i + 1))
        if (freeFingerTypes[i]) {
            freeFingerTypes[i] = false;
            vf.fingerType = i;
            break;
        }
}

void ALPS::freeAndMarkVirtualFingers() {
    memset(freeFingerTypes, true, kMT2FingerTypeCount);
    freeFingerTypes[kMT2FingerTypeUndefined] = false;

    for (int i = 0; i < MAX_TOUCHES; i++) { // free up all virtual fingers
        auto &vfi = virtualFingerStates[i];
        vfi.touch = false;
        vfi.x_avg.reset(); // maybe it should be done only for unpressed fingers?
        vfi.y_avg.reset();
        vfi.pressure = 0;
    }

    for (int i = 0; i < clampedFingerCount; i++) { // mark virtual fingers as used
        int j = fingerStates[i].virtualFingerIndex;
        if (j == -1) {
            IOLog("alps_parse_hw_state: WTF!? Finger %d has no virtual finger\n", i);
            continue;
        }
        auto &vfj = virtualFingerStates[j];
        vfj.touch = true;
        freeFingerTypes[vfj.fingerType] = false;
    }

    for (int i = 0; i < MAX_TOUCHES; i++) {
        auto &vfi = virtualFingerStates[i];
        if (!vfi.touch)
            vfi.fingerType = kMT2FingerTypeUndefined;
    }
}

static void clone(alps_hw_state &dst, const alps_hw_state &src) {
    dst.x = src.x;
    dst.y = src.y;
    dst.z = src.z;
}

int ALPS::upperFingerIndex() const {
    return fingerStates[0].y < fingerStates[1].y ? 1 : 0;
}

const alps_hw_state& ALPS::upperFinger() const {
    return fingerStates[upperFingerIndex()];
}

void ALPS::swapFingers(int dst, int src) {
    int j = fingerStates[src].virtualFingerIndex;
    const auto &vfj = virtualFingerStates[j];
    fingerStates[dst].x = vfj.x_avg.average();
    fingerStates[dst].y = vfj.y_avg.average();
    fingerStates[dst].virtualFingerIndex = j;
    assignVirtualFinger(src);
}

#define FINGER_DIST 1000000

bool ALPS::renumberFingers() {
    const auto &f0 = fingerStates[0];
    const auto &f1 = fingerStates[1];
    auto &f2 = fingerStates[2];
    auto &f3 = fingerStates[3];
    auto &f4 = fingerStates[4];

    if (clampedFingerCount == lastFingerCount && clampedFingerCount >= 3) {
        // update imaginary finger states
        if (f0.virtualFingerIndex != -1 && f1.virtualFingerIndex != -1) {
            if (clampedFingerCount >= 4) {
                const auto &fi = upperFinger();
                const auto &fiv = virtualFingerStates[fi.virtualFingerIndex];
                for (int j = 2; j < clampedFingerCount; j++) {
                    auto &fj = fingerStates[j];
                    fj.x += fi.x - fiv.x_avg.newest();
                    fj.y += fi.y - fiv.y_avg.newest();
                    fj.z = fi.z;
                }
            }
            else if (clampedFingerCount == 3) {
                const auto &f0v = virtualFingerStates[f0.virtualFingerIndex];
                const auto &f1v = virtualFingerStates[f1.virtualFingerIndex];
                auto &f2 = fingerStates[2];
                f2.x += ((f0.x - f0v.x_avg.newest()) + (f1.x - f1v.x_avg.newest())) / 2;
                f2.y += ((f0.y - f0v.y_avg.newest()) + (f1.y - f1v.y_avg.newest())) / 2;
                f2.z = (f0.z + f1.z) / 2;
            }
        }
        else
            IOLog("alps_parse_hw_state: WTF - have %d fingers, but first 2 don't have virtual finger\n", clampedFingerCount);
    }

    // We really need to send the "no touch" event
    // multiple times, because if we don't do it and return,
    // gestures like desktop switching or inertial scrolling
    // got stuck midway until the next touch.
    //if(!lastFingerCount && !clampedFingerCount) {
    //    return 0;
    //}

    // Finger type detection:
    // We think that fingers are added beginning with the index finger,
    // then middle, ring and little.
    // However, when the finger count reaches 4, the lowest finger becomes thumb,
    // but other fingers don't change their types.
    // All fingers preserve their types during the gesture.
    // Though it would be nice to see what MT2 does.

    if (clampedFingerCount == lastFingerCount && clampedFingerCount == 1) {
        int i = 0;
        int j = fingerStates[i].virtualFingerIndex;
        int d = dist(i, j);
        if (d > FINGER_DIST) { // this number was determined experimentally
            // Prevent jumps by unpressing finger. Other way could be leaving the old finger pressed.
            DEBUG_LOG("alps_parse_hw_state: unpressing finger: dist is %d\n", d);
            auto &vfj = virtualFingerStates[j];
            vfj.x_avg.reset();
            vfj.y_avg.reset();
            vfj.pressure = 0;
            vfj.fingerType = kMT2FingerTypeUndefined;
            clampedFingerCount = 0;
        }
    }
    if (clampedFingerCount != lastFingerCount) {
        if (clampedFingerCount > lastFingerCount && clampedFingerCount >= 3) {
            // Skip sending touch data once because we need to wait for the next extended packet
            if (wasSkipped)
                wasSkipped = false;
            else {
                DEBUG_LOG("alps_parse_hw_state: Skip sending touch data\n");
                wasSkipped = true;
                return false;
            }
        }

        if (lastFingerCount == 0) {
            // Assign to identity mapping
            for (int i = 0; i < clampedFingerCount; i++) {
                auto &fi = fingerStates[i];
                fi.virtualFingerIndex = i;
                auto &vfi = virtualFingerStates[i];
                vfi.touch = true;
                assignFingerType(vfi);
                vfi.x_avg.reset();
                vfi.y_avg.reset();
                if (i >= 2) // more than 3 fingers added simultaneously
                    clone(fi, upperFinger()); // Copy from the upper finger
            }
        }
        else if (clampedFingerCount > lastFingerCount && !hadLiftFinger) {
            // First finger already exists
            // Can add 1, 2 or 3 fingers at once
            // Newly added finger is always in secondary finger packet
            switch (clampedFingerCount - lastFingerCount) {
                case 1:
                    if (lastFingerCount >= 2)
                        swapFingers(lastFingerCount, 1);
                    else // lastFingerCount = 1
                        assignVirtualFinger(1);
                    break;
                case 2:
                    if (lastFingerCount == 1) { // added second and third
                        assignVirtualFinger(1);
                        clone(f2, upperFinger()); // We don't know better
                        assignVirtualFinger(2);
                    }
                    else { // added third and fourth
                        swapFingers(lastFingerCount, 1);

                        // add fourth
                        clone(f3, upperFinger());
                        assignVirtualFinger(3);
                    }
                    break;
                case 3:
                    assignVirtualFinger(1);
                    clone(f2, upperFinger());
                    assignVirtualFinger(2);
                    clone(f3, upperFinger());
                    assignVirtualFinger(3);
                    break;
                case 4:
                    assignVirtualFinger(1);
                    clone(f2, upperFinger());
                    assignVirtualFinger(2);
                    clone(f3, upperFinger());
                    assignVirtualFinger(3);
                    clone(f4, upperFinger());
                    assignVirtualFinger(4);
                    break;
                default:
                    IOLog("alps_parse_hw_state: WTF!? fc=%d lfc=%d\n", clampedFingerCount, lastFingerCount);
            }
        }
        else if (clampedFingerCount > lastFingerCount && hadLiftFinger) {
            for (int i = 0; i < MAX_TOUCHES; i++) // clean virtual finger numbers
                fingerStates[i].virtualFingerIndex = -1;

            int maxMinDist = 0, maxMinDistIndex = -1;
            int secondMaxMinDist = 0, secondMaxMinDistIndex = -1;

            // find new physical finger for each existing virtual finger
            for (int j = 0; j < MAX_TOUCHES; j++) {
                if (!virtualFingerStates[j].touch)
                    continue; // free
                int minDist = INT_MAX, minIndex = -1;
                for (int i = 0; i < lastFingerCount; i++) {
                    if (fingerStates[i].virtualFingerIndex != -1)
                        continue; // already taken
                    int d = dist(i, j);
                    if (d < minDist) {
                        minDist = d;
                        minIndex = i;
                    }
                }
                if (minIndex == -1) {
                    IOLog("alps_parse_hw_state: WTF!? minIndex is -1\n");
                    continue;
                }
                if (minDist > maxMinDist) {
                    secondMaxMinDist = maxMinDist;
                    secondMaxMinDistIndex = maxMinDistIndex;
                    maxMinDist = minDist;
                    maxMinDistIndex = minIndex;
                }
                fingerStates[minIndex].virtualFingerIndex = j;
            }

            // assign new virtual fingers for all new fingers
            for (int i = 0; i < min(2, clampedFingerCount); i++) // third and fourth 'fingers' are handled separately
                if (fingerStates[i].virtualFingerIndex == -1)
                    assignVirtualFinger(i); // here OK

            if (clampedFingerCount == 3) {
                DEBUG_LOG("alps_parse_hw_state: adding third finger, maxMinDist=%d\n", maxMinDist);
                f2.z = (f0.z + f1.z) / 2;
                if (maxMinDist > FINGER_DIST && maxMinDistIndex >= 0) {
                    // i-th physical finger was replaced, save its old coordinates to the 3rd physical finger and map it to a new virtual finger.
                    // The third physical finger should now be mapped to the old fingerStates[i].virtualFingerIndex.
                    swapFingers(2, maxMinDistIndex);
                    DEBUG_LOG("alps_parse_hw_state: swapped, saving location\n");
                }
                else {
                    // existing fingers didn't change or were swapped, so we don't know the location of the third finger
                    const auto &fj = upperFinger();

                    f2.x = fj.x;
                    f2.y = fj.y;
                    assignVirtualFinger(2);
                    DEBUG_LOG("alps_parse_hw_state: not swapped, taking upper finger position\n");
                }
            }
            else if (clampedFingerCount >= 4) {
                // Is it possible that both 0 and 1 fingers were swapped with 2 and 3?
                DEBUG_LOG("alps_parse_hw_state: adding third and fourth fingers, maxMinDist=%d, secondMaxMinDist=%d\n", maxMinDist, secondMaxMinDist);
                f2.z = f3.z = (f0.z + f1.z) / 2;

                // Possible situations:
                // 1. maxMinDist ≤ 1000000, lastFingerCount = 3 - no fingers swapped, just adding 4th finger
                // 2. maxMinDist ≤ 1000000, lastFingerCount = 2 - no fingers swapped, just adding 3rd and 4th fingers
                // 3. maxMinDist > 1000000, secondMaxMinDist ≤ 1000000, lastFingerCount = 3 - i'th finger was swapped with 4th, 3rd left in place (i∈{0,1}):
                //      4th.xy = i'th.xy
                //      p2v[2] = j
                //      p2v[i] = next free
                // 4. maxMinDist > 1000000, secondMaxMinDist > 1000000, lastFingerCount = 3 - i'th finger was swapped with 3rd and k'th finger was swapped with 4th (i,k∈{0,1}):
                //      is it possible that only imaginary finger was left in place?!
                // 5. maxMinDist > 1000000, secondMaxMinDist ≤ 1000000, lastFingerCount = 2 - one finger swapped, one finger left in place.


                if (maxMinDist > FINGER_DIST && maxMinDistIndex >= 0) {
                    if (lastFingerCount < 3) {
                        // i-th physical finger was replaced, save its old coordinates to the 3rd physical finger and map it to a new virtual finger.
                        // The third physical finger should now be mapped to the old fingerStates[i].virtualFingerIndex.
                        swapFingers(2, maxMinDistIndex);
                        if (secondMaxMinDist > FINGER_DIST && secondMaxMinDistIndex >= 0) {
                            // both fingers were swapped with new ones
                            // i-th physical finger was replaced, save its old coordinates to the 4th physical finger and map it to a new virtual finger.
                            // The fourth physical finger should now be mapped to the old fingerStates[i].virtualFingerIndex.
                            swapFingers(3, secondMaxMinDistIndex);
                        }
                        else {
                            // fourth finger is new
                            clone(f3, upperFinger());
                            assignVirtualFinger(3);
                        }
                    }
                    else {
                        // i-th physical finger was replaced, save its old coordinates to the 4th physical finger and map it to a new virtual finger.
                        // The fourth physical finger should now be mapped to the old fingerStates[i].virtualFingerIndex.
                        swapFingers(3, maxMinDistIndex);
                        if (secondMaxMinDist > FINGER_DIST && secondMaxMinDistIndex >= 0) {
                            IOLog("alps_parse_hw_state: WTF, I thought it is impossible: fc=%d, lfc=%d, mdi=%d(%d), smdi=%d(%d)\n", clampedFingerCount, lastFingerCount, maxMinDist, maxMinDistIndex, secondMaxMinDist, secondMaxMinDistIndex);
                        }
                    }
                    DEBUG_LOG("alps_parse_hw_state: swapped, saving location\n");
                }
                else {
                    // existing fingers didn't change or were swapped, so we don't know the location of the third and fourth fingers
                    const auto &fj = upperFinger();
                    clone(f2, fj);
                    if (lastFingerCount < 3)
                        assignVirtualFinger(2);
                    clone(f3, fj);
                    assignVirtualFinger(3);
                    DEBUG_LOG("alps_parse_hw_state: not swapped, cloning existing fingers\n");
                }
                if (clampedFingerCount >= 5) {
                    // Don't bother with 5th finger, always clone
                    clone(f4, upperFinger());
                    assignVirtualFinger(4);
                    DEBUG_LOG("alps_parse_hw_state: cloning 5th finger\n");
                }
            }
            freeAndMarkVirtualFingers();
        }
        else if (clampedFingerCount < lastFingerCount) {
            // Set hadLiftFinger if lifted some fingers
            // Reset hadLiftFinger if lifted all fingers
            hadLiftFinger = clampedFingerCount > 0;

            // some fingers removed, need renumbering
            bool used[MAX_TOUCHES];
            for (int i = 0; i < MAX_TOUCHES; i++) { // clean virtual finger numbers
                fingerStates[i].virtualFingerIndex = -1;
                used[i] = false;
            }
            for (int i = 0; i < clampedFingerCount; i++) {
                // find new virtual finger number with nearest coordinates for this finger
                int minDist = INT_MAX, minIndex = -1;
                for (int j = 0; j < MAX_TOUCHES; j++) {
                    if (!virtualFingerStates[j].touch || used[j])
                        continue;
                    int d = dist(i, j);
                    if (d < minDist) {
                        minDist = d;
                        minIndex = j;
                    }
                }
                fingerStates[i].virtualFingerIndex = minIndex;
                if (minIndex == -1) {
                    IOLog("alps_parse_hw_state: WTF: renumbering failed, minIndex for %d is -1\n", i);
                    continue;
                }
                used[minIndex] = true;
            }
            freeAndMarkVirtualFingers();
        }
    }

    for (int i = 0; i < clampedFingerCount; i++) {
        const auto &fi = fingerStates[i];
        DEBUG_LOG("alps_parse_hw_state: finger %d -> virtual finger %d\n", i, fi.virtualFingerIndex);
        if (fi.virtualFingerIndex < 0 || fi.virtualFingerIndex >= MAX_TOUCHES) {
            IOLog("alps_parse_hw_state: ERROR: invalid physical finger %d\n", fi.virtualFingerIndex);
            continue;
        }
        virtual_finger_state &fiv = virtualFingerStates[fi.virtualFingerIndex];
        fiv.x_avg.filter(fi.x);
        fiv.y_avg.filter(fi.y);
        fiv.pressure = fi.z;
        // Only use this if trackpad is a clickpad
        if (priv.flags & ALPS_BUTTONPAD)
            fiv.button = left;
        else
            fiv.button = 0;
    }

    // Thumb detection. Must happen after setting coordinates (filter)
    if (clampedFingerCount > lastFingerCount && clampedFingerCount >= 4) {
        // find the lowest finger
        int lowestFingerIndex = -1;
        int min_y = INT_MAX;
        for (int i = 0; i < MAX_TOUCHES; i++) {
            const auto &vfi = virtualFingerStates[i];
            DEBUG_LOG("alps_parse_hw_state: finger %d: touch %d, y %d\n", i, vfi.touch, vfi.y_avg.average());
            if (vfi.touch && vfi.y_avg.average() < min_y) {
                lowestFingerIndex = i;
                min_y = vfi.y_avg.average();
            }
        }
        DEBUG_LOG("alps_parse_hw_state: lowest finger: %d\n", lowestFingerIndex);
        if (lowestFingerIndex == -1)
            IOLog("alps_parse_hw_state: WTF?! lowest finger not found!\n");
        else {
            auto &vf = virtualFingerStates[lowestFingerIndex];
            freeFingerTypes[vf.fingerType] = true;
            vf.fingerType = kMT2FingerTypeThumb;
            freeFingerTypes[kMT2FingerTypeThumb] = false;
        }
    }

    DEBUG_LOG("alps_parse_hw_state: lastFingerCount=%d clampedFingerCount=%d left=%d\n", lastFingerCount,  clampedFingerCount, left);
    return true;
}

void ALPS::sendTouchData() {
    // Ignore input for specified time after keyboard usage
    AbsoluteTime timestamp;
    clock_get_uptime(&timestamp);
    uint64_t timestamp_ns;
    absolutetime_to_nanoseconds(timestamp, &timestamp_ns);

    if (timestamp_ns - keytime < maxaftertyping)
        return;

    if (lastFingerCount != clampedFingerCount) {
        lastFingerCount = clampedFingerCount;
        return; // Skip while fingers are placed on the touchpad or removed
    }

    static_assert(VOODOO_INPUT_MAX_TRANSDUCERS >= MAX_TOUCHES, "Trackpad supports too many fingers");

    int transducers_count = 0;
    for(int i = 0; i < MAX_TOUCHES; i++) {
        const auto& state = virtualFingerStates[i];
        if (!state.touch)
            continue;

        auto& transducer = inputEvent.transducers[transducers_count++];

        transducer.type = FINGER;
        transducer.isValid = true;
        if (_forceTouchMode == FORCE_TOUCH_DISABLED || _forceTouchMode == FORCE_TOUCH_BUTTON)
            transducer.supportsPressure = false;
        else
            transducer.supportsPressure = true;

        int posX = state.x_avg.average();
        int posY = state.y_avg.average();

        posY = logical_max_y + 1 - posY;

        DEBUG_LOG("alps_parse_hw_state: finger[%d] x=%d y=%d raw_x=%d raw_y=%d\n", i, posX, posY, state.x_avg.average(), state.y_avg.average());

        transducer.previousCoordinates = transducer.currentCoordinates;

        transducer.currentCoordinates.x = posX;
        transducer.currentCoordinates.y = posY;
        transducer.timestamp = timestamp;

        switch (_forceTouchMode)
        {
            case FORCE_TOUCH_BUTTON: // Physical button is translated into force touch instead of click
                transducer.isPhysicalButtonDown = false;
                transducer.currentCoordinates.pressure = state.button ? 255 : 0;
                break;

            case FORCE_TOUCH_THRESHOLD: // Force touch is touch with pressure over threshold
                transducer.isPhysicalButtonDown = state.button;
                transducer.currentCoordinates.pressure = state.pressure > _forceTouchPressureThreshold ? 255 : 0;
                break;

            case FORCE_TOUCH_VALUE: // Pressure is passed to system as is
                transducer.isPhysicalButtonDown = state.button;
                transducer.currentCoordinates.pressure = state.pressure;
                break;

            case FORCE_TOUCH_CUSTOM: // Pressure is passed, but with locking
                transducer.isPhysicalButtonDown = state.button;

                if (clampedFingerCount != 1) {
                    transducer.currentCoordinates.pressure = state.pressure > _forceTouchPressureThreshold ? 255 : 0;
                    break;
                }

                double value;
                if (state.pressure >= _forceTouchCustomDownThreshold) {
                    value = 1.0;
                } else if (state.pressure <= _forceTouchCustomUpThreshold) {
                    value = 0.0;
                } else {
                    double base = ((double) (state.pressure - _forceTouchCustomUpThreshold)) / ((double) (_forceTouchCustomDownThreshold - _forceTouchCustomUpThreshold));
                    value = 1;
                    for (int i = 0; i < _forceTouchCustomPower; ++i) {
                        value *= base;
                    }
                }
                transducer.currentCoordinates.pressure = (int) (value * 255);
                break;

            case FORCE_TOUCH_DISABLED:
            default:
                transducer.isPhysicalButtonDown = state.button;
                transducer.currentCoordinates.pressure = 0;
                break;

        }

        transducer.isTransducerActive = 1;
        transducer.currentCoordinates.width = state.pressure / 2;
        if (state.fingerType == kMT2FingerTypeUndefined)
            IOLog("alps_parse_hw_state: WTF!? finger type is undefined\n");
        if (state.fingerType < kMT2FingerTypeUndefined || state.fingerType > kMT2FingerTypeLittleFinger)
            IOLog("alps_parse_hw_state: WTF!? finger type is out of range\n");
        if (freeFingerTypes[state.fingerType])
            IOLog("alps_parse_hw_state: WTF!? finger type is marked free\n");
        transducer.fingerType = state.fingerType;
        transducer.secondaryId = i;
    }

    for (int i = 0; i < transducers_count; i++)
        for (int j = i + 1; j < transducers_count; j++)
            if (inputEvent.transducers[i].fingerType == inputEvent.transducers[j].fingerType)
                IOLog("alps_parse_hw_state: WTF!? equal finger types\n");

    if (transducers_count != clampedFingerCount)
        IOLog("alps_parse_hw_state: WTF?! tducers_count %d clampedFingerCount %d\n", transducers_count, clampedFingerCount);

    // create new VoodooI2CMultitouchEvent
    inputEvent.contact_count = transducers_count;
    inputEvent.timestamp = timestamp;

    // send the event into the multitouch interface
    // send the 0 finger message only once
    if (inputEvent.contact_count != 0 || lastSentFingerCount != 0) {
        super::messageClient(kIOMessageVoodooInputMessage, voodooInputInstance, &inputEvent, sizeof(VoodooInputEvent));
    }
    lastFingerCount = clampedFingerCount;
    lastSentFingerCount = inputEvent.contact_count;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

void ALPS::initTouchPad() {
    //
    // Clear packet buffer pointer to avoid issues caused by
    // stale packet fragments.
    //

    _packetByteCount = 0;
    _ringBuffer.reset();

    // clear state of control key cache
    _modifierdown = 0;

    // initialize the touchpad
    deviceSpecificInit();
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

void ALPS::setParamPropertiesGated(OSDictionary * config) {
    if (NULL == config)
        return;

    const struct {const char *name; int *var;} int32vars[]={
        {"FingerZ",                         &z_finger},
        {"WakeDelay",                       &wakedelay},
        {"Resolution",                      &_resolution},
        {"ScrollResolution",                &_scrollresolution},
        {"UnitsPerMMX",                     &xupmm},
        {"UnitsPerMMY",                     &yupmm},
        {"MinLogicalXOverride",             &minXOverride},
        {"MinLogicalYOverride",             &minYOverride},
        {"MaxLogicalXOverride",             &maxXOverride},
        {"MaxLogicalYOverride",             &maxYOverride},
        {"ForceTouchMode",                  (int*)&_forceTouchMode}, // 0 - disable, 1 - left button, 2 - pressure threshold, 3 - pass pressure value
        {"ForceTouchPressureThreshold",     &_forceTouchPressureThreshold}, // used in mode 2
        {"ForceTouchCustomDownThreshold",   &_forceTouchCustomDownThreshold}, // used in mode 4
        {"ForceTouchCustomUpThreshold",     &_forceTouchCustomUpThreshold}, // used in mode 4
        {"ForceTouchCustomPower",           &_forceTouchCustomPower}, // used in mode 4
    };

    const struct {const char *name; int *var;} boolvars[]={
        {"ProcessUSBMouseStopsTrackpad",    &_processusbmouse},
        {"ProcessBluetoothMouseStopsTrackpad", &_processbluetoothmouse},
    };

    const struct {const char* name; bool* var;} lowbitvars[]={
        {"USBMouseStopsTrackpad",           &usb_mouse_stops_trackpad},
    };
    const struct {const char* name; uint64_t* var; } int64vars[]={
        {"QuietTimeAfterTyping",            &maxaftertyping},
    };

    OSBoolean *bl;
    OSNumber *num;
    // 64-bit config items
    for (int i = 0; i < countof(int64vars); i++)
        if ((num=OSDynamicCast(OSNumber, config->getObject(int64vars[i].name))))
        {
            *int64vars[i].var = num->unsigned64BitValue();
            setProperty(int64vars[i].name, *int64vars[i].var, 64);
        }
    // boolean config items
    for (int i = 0; i < countof(boolvars); i++)
        if ((bl=OSDynamicCast (OSBoolean,config->getObject (boolvars[i].name))))
        {
            *boolvars[i].var = bl->isTrue();
            setProperty(boolvars[i].name, *boolvars[i].var ? kOSBooleanTrue : kOSBooleanFalse);
        }
    // 32-bit config items
    for (int i = 0; i < countof(int32vars);i++)
        if ((num=OSDynamicCast (OSNumber,config->getObject (int32vars[i].name))))
        {
            *int32vars[i].var = num->unsigned32BitValue();
            setProperty(int32vars[i].name, *int32vars[i].var, 32);
        }
    // lowbit config items
    for (int i = 0; i < countof(lowbitvars); i++)
    {
        if ((num=OSDynamicCast (OSNumber,config->getObject(lowbitvars[i].name))))
        {
            *lowbitvars[i].var = (num->unsigned32BitValue()&0x1)?true:false;
            setProperty(lowbitvars[i].name, *lowbitvars[i].var ? 1 : 0, 32);
        }
        //REVIEW: are these items ever carried in a boolean?
        else if ((bl=OSDynamicCast(OSBoolean, config->getObject(lowbitvars[i].name))))
        {
            *lowbitvars[i].var = bl->isTrue();
            setProperty(lowbitvars[i].name, *lowbitvars[i].var ? kOSBooleanTrue : kOSBooleanFalse);
        }
    }

    // disable trackpad when USB mouse is plugged in and this functionality is requested
    if (attachedHIDPointerDevices && attachedHIDPointerDevices->getCount() > 0) {
        ignoreall = usb_mouse_stops_trackpad;
    }

    if (_forceTouchMode == FORCE_TOUCH_BUTTON) {
        int val[16];
        if (PE_parse_boot_argn("rp0", val, sizeof(val)) ||
            PE_parse_boot_argn("rp", val, sizeof(val)) ||
            PE_parse_boot_argn("container-dmg", val, sizeof(val)) ||
            PE_parse_boot_argn("root-dmg", val, sizeof(val)) ||
            PE_parse_boot_argn("auth-root-dmg", val, sizeof(val)))
            _forceTouchMode = FORCE_TOUCH_DISABLED;
    }
}

IOReturn ALPS::setParamProperties(OSDictionary* dict) {
    ////IOReturn result = super::IOHIDevice::setParamProperties(dict);
    if (_cmdGate)
    {
        // syncronize through workloop...
        ////_cmdGate->runAction(OSMemberFunctionCast(IOCommandGate::Action, this, &VooodooPS2TouchPadBase::setParamPropertiesGated), dict);
        setParamPropertiesGated(dict);
    }

    return super::setParamProperties(dict);
    ////return result;
}

IOReturn ALPS::setProperties(OSObject *props) {
    OSDictionary *dict = OSDynamicCast(OSDictionary, props);
    if (dict && _cmdGate)
    {
        // syncronize through workloop...
        _cmdGate->runAction(OSMemberFunctionCast(IOCommandGate::Action, this, &ALPS::setParamPropertiesGated), dict);
    }

    return super::setProperties(props);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

void ALPS::setDevicePowerState( UInt32 whatToDo ) {
    switch ( whatToDo )
    {
        case kPS2C_DisableDevice:
            //
            // Disable touchpad (synchronous).
            //

            setTouchPadEnable( false );
            break;

        case kPS2C_EnableDevice:
            //
            // Must not issue any commands before the device has
            // completed its power-on self-test and calibration.
            //

            IOSleep(wakedelay);

            // MARK: Find another way to fix trackpad breaking on V8 after sleep
            // This workaround is very messy and unstable.
            // A proper fix is needed.
            // Reset and re-initialize touchpad
            _device->lock();
            resetMouse();
            identify();
            initTouchPad();
            _device->unlock();
            break;
    }
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

IOReturn ALPS::message(UInt32 type, IOService* provider, void* argument) {
    //
    // Here is where we receive messages from the keyboard driver
    //
    // This allows for the keyboard driver to enable/disable the trackpad
    // when a certain keycode is pressed.
    //
    // It also allows the trackpad driver to learn the last time a key
    // has been pressed, so it can implement various "ignore trackpad
    // input while typing" options.
    //

    switch (type)
    {
        case kPS2M_getDisableTouchpad:
        {
            bool* pResult = (bool*)argument;
            *pResult = !ignoreall;
            break;
        }

        case kPS2M_setDisableTouchpad:
        {
            bool enable = *((bool*)argument);
            // ignoreall is true when trackpad has been disabled
            if (enable == ignoreall)
            {
                // save state, and update LED
                ignoreall = !enable;
            }
            break;
        }

        case kPS2M_resetTouchpad:
        {
            int *reqCode = (int *)argument;
            DEBUG_LOG("ALPS::kPS2M_resetTouchpad reqCode: %d\n", *reqCode);
            if (*reqCode == 1) {
                ignoreall = false;
                _device->lock();
                resetMouse();
                IOSleep(wakedelay);
                identify();
                initTouchPad();
                _device->unlock();
            }
            break;
        }

        case kPS2M_notifyKeyPressed:
        {
            // just remember last time key pressed... this can be used in
            // interrupt handler to detect unintended input while typing
            PS2KeyInfo* pInfo = (PS2KeyInfo*)argument;
            static const int masks[] =
            {
                0x10,       // 0x36
                0x100000,   // 0x37
                0,          // 0x38
                0,          // 0x39
                0x080000,   // 0x3a
                0x040000,   // 0x3b
                0,          // 0x3c
                0x08,       // 0x3d
                0x04,       // 0x3e
                0x200000,   // 0x3f
            };

            switch (pInfo->adbKeyCode)
            {
                // don't store key time for modifier keys going down
                // track modifiers for scrollzoom feature...
                // (note: it turns out we didn't need to do this, but leaving this code in for now in case it is useful)
                case 0x38:  // left shift
                case 0x3c:  // right shift
                case 0x3b:  // left control
                case 0x3e:  // right control
                case 0x3a:  // left windows (option)
                case 0x3d:  // right windows
                case 0x37:  // left alt (command)
                case 0x36:  // right alt
                case 0x3f:  // osx fn (function)
                    if (pInfo->goingDown)
                    {
                        _modifierdown |= masks[pInfo->adbKeyCode-0x36];
                        break;
                    }
                    _modifierdown &= ~masks[pInfo->adbKeyCode-0x36];
                    keytime = pInfo->time;
                    break;

                default:
                    keytime = pInfo->time;
            }
            break;
        }
    }

    return kIOReturnSuccess;
}

void ALPS::registerHIDPointerNotifications() {
    IOServiceMatchingNotificationHandler notificationHandler = OSMemberFunctionCast(IOServiceMatchingNotificationHandler, this, &ALPS::notificationHIDAttachedHandler);

    // Determine if we should listen for USB mouse attach events as per configuration
    if (_processusbmouse) {
        // USB mouse HID description as per USB spec: http://www.usb.org/developers/hidpage/HID1_11.pdf
        OSDictionary* matchingDictionary = serviceMatching("IOUSBInterface");

        propertyMatching(OSSymbol::withCString(kUSBHostMatchingPropertyInterfaceClass), OSNumber::withNumber(kUSBHIDInterfaceClass, 8), matchingDictionary);
        propertyMatching(OSSymbol::withCString(kUSBHostMatchingPropertyInterfaceSubClass), OSNumber::withNumber(kUSBHIDBootInterfaceSubClass, 8), matchingDictionary);
        propertyMatching(OSSymbol::withCString(kUSBHostMatchingPropertyInterfaceProtocol), OSNumber::withNumber(kHIDMouseInterfaceProtocol, 8), matchingDictionary);

        // Register for future services
        usb_hid_publish_notify = addMatchingNotification(gIOFirstPublishNotification, matchingDictionary, notificationHandler, this, NULL, 10000);
        usb_hid_terminate_notify = addMatchingNotification(gIOTerminatedNotification, matchingDictionary, notificationHandler, this, NULL, 10000);
        OSSafeReleaseNULL(matchingDictionary);
    }

    // Determine if we should listen for bluetooth mouse attach events as per configuration
    if (_processbluetoothmouse) {
        // Bluetooth HID devices
        OSDictionary* matchingDictionary = serviceMatching("IOBluetoothHIDDriver");
        propertyMatching(OSSymbol::withCString(kIOHIDVirtualHIDevice), kOSBooleanFalse, matchingDictionary);

        // Register for future services
        bluetooth_hid_publish_notify = addMatchingNotification(gIOFirstPublishNotification, matchingDictionary, notificationHandler, this, NULL, 10000);
        bluetooth_hid_terminate_notify = addMatchingNotification(gIOTerminatedNotification, matchingDictionary, notificationHandler, this, NULL, 10000);
        OSSafeReleaseNULL(matchingDictionary);
    }
}

void ALPS::unregisterHIDPointerNotifications() {
    // Free device matching notifiers
    // remove() releases them
    if (usb_hid_publish_notify)
        usb_hid_publish_notify->remove();

    if (usb_hid_terminate_notify)
        usb_hid_terminate_notify->remove();

    if (bluetooth_hid_publish_notify)
        bluetooth_hid_publish_notify->remove();

    if (bluetooth_hid_terminate_notify)
        bluetooth_hid_terminate_notify->remove();

    attachedHIDPointerDevices->flushCollection();
}

void ALPS::notificationHIDAttachedHandlerGated(IOService * newService, IONotifier * notifier) {
    char path[256];
    int len = 255;
    memset(path, 0, len);
    newService->getPath(path, &len, gIOServicePlane);

    if (notifier == usb_hid_publish_notify) {
        attachedHIDPointerDevices->setObject(newService);
        DEBUG_LOG("%s: USB pointer HID device published: %s, # devices: %d\n", getName(), path, attachedHIDPointerDevices->getCount());
    }

    if (notifier == usb_hid_terminate_notify) {
        attachedHIDPointerDevices->removeObject(newService);
        DEBUG_LOG("%s: USB pointer HID device terminated: %s, # devices: %d\n", getName(), path, attachedHIDPointerDevices->getCount());
    }

    if (notifier == bluetooth_hid_publish_notify) {

        // Filter on specific CoD (Class of Device) bluetooth devices only
        OSNumber* propDeviceClass = OSDynamicCast(OSNumber, newService->getProperty("ClassOfDevice"));

        if (propDeviceClass != NULL) {

            long classOfDevice = propDeviceClass->unsigned32BitValue();

            long deviceClassMajor = (classOfDevice & 0x1F00) >> 8;
            long deviceClassMinor = (classOfDevice & 0xFF) >> 2;

            if (deviceClassMajor == kBluetoothDeviceClassMajorPeripheral) { // Bluetooth peripheral devices

                long deviceClassMinor1 = (deviceClassMinor) & 0x30;
                long deviceClassMinor2 = (deviceClassMinor) & 0x0F;

                if (deviceClassMinor1 == kBluetoothDeviceClassMinorPeripheral1Pointing || // Seperate pointing device
                    deviceClassMinor1 == kBluetoothDeviceClassMinorPeripheral1Combo) // Combo bluetooth keyboard/touchpad
                {
                    if (deviceClassMinor2 == kBluetoothDeviceClassMinorPeripheral2Unclassified || // Mouse
                        deviceClassMinor2 == kBluetoothDeviceClassMinorPeripheral2DigitizerTablet || // Magic Touchpad
                        deviceClassMinor2 == kBluetoothDeviceClassMinorPeripheral2DigitalPen) // Wacom Tablet
                    {

                        attachedHIDPointerDevices->setObject(newService);
                        DEBUG_LOG("%s: Bluetooth pointer HID device published: %s, # devices: %d\n", getName(), path, attachedHIDPointerDevices->getCount());
                    }
                }
            }
        }
    }

    if (notifier == bluetooth_hid_terminate_notify) {
        attachedHIDPointerDevices->removeObject(newService);
        DEBUG_LOG("%s: Bluetooth pointer HID device terminated: %s, # devices: %d\n", getName(), path, attachedHIDPointerDevices->getCount());
    }

    if (notifier == usb_hid_publish_notify || notifier == bluetooth_hid_publish_notify) {
        if (usb_mouse_stops_trackpad && attachedHIDPointerDevices->getCount() > 0) {
            // One or more USB or Bluetooth pointer devices attached, disable trackpad
            ignoreall = true;
        }
    }

    if (notifier == usb_hid_terminate_notify || notifier == bluetooth_hid_terminate_notify) {
        if (usb_mouse_stops_trackpad && attachedHIDPointerDevices->getCount() == 0) {
            // No USB or bluetooth pointer devices attached, re-enable trackpad
            ignoreall = false;
        }
    }
}

bool ALPS::notificationHIDAttachedHandler(void * refCon, IOService * newService, IONotifier * notifier) {
    if (_cmdGate) { // defensive
        _cmdGate->runAction(OSMemberFunctionCast(IOCommandGate::Action, this, &ALPS::notificationHIDAttachedHandlerGated), newService, notifier);
    }

    return true;
}
