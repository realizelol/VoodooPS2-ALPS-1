/*
 * Intel ACPI Component Architecture
 * AML/ASL+ Disassembler version 20200925 (64-bit version)
 * Copyright (c) 2000 - 2020 Intel Corporation
 * 
 * Disassembling to symbolic ASL+ operators
 *
 * Disassembly of iASLkw8W0P.aml, Sat Feb  5 21:18:41 2022
 *
 * Original Table Header:
 *     Signature        "SSDT"
 *     Length           0x0000013D (317)
 *     Revision         0x02
 *     Checksum         0x9A
 *     OEM ID           "HACK"
 *     OEM Table ID     "PS2K"
 *     OEM Revision     0x00000000 (0)
 *     Compiler ID      "INTL"
 *     Compiler Version 0x20200925 (538970405)
 */
DefinitionBlock ("", "SSDT", 2, "HACK", "PS2K", 0x00000000)
{
    External (_SB_.PCI0.LPCB.PS2K, DeviceObj)
    External (_SB_.PCI0.LPCB.PS2M, DeviceObj)

    Scope (\_SB.PCI0.LPCB.PS2K)
    {
        Method(_DSM, 4)
        {
            If (!Arg2) { Return (Buffer() { 0x03 } ) }
            Return (Package()
            {
                "QuietTimeAfterTyping", 0x0,
                "RM,deliverNotifications", ">n",
            })
        }
        
        Name (RMCF, Package ()
        {
            "Controller", 
            Package ()
            {
                "QuietTimeAfterTyping", 0x0,
                "RM,deliverNotifications", ">n"
            },
            "ALPS", 
            Package ()
            {
                "QuietTimeAfterTyping", 0x0,
                "RM,deliverNotifications", ">n"
            },
            "ALPS GlidePoint", 
            Package ()
            {
                "QuietTimeAfterTyping", 0x0,
                "RM,deliverNotifications", ">n"
            },
            "Trackpad", 
            Package ()
            {
                "QuietTimeAfterTyping", 0x0,
                "RM,deliverNotifications", ">n"
            },
            "Mouse", 
            Package ()
            {
                "QuietTimeAfterTyping", 0x0,
                "RM,deliverNotifications", ">n"
            }
        })
    }

    Scope (\_SB.PCI0.LPCB.PS2M)
    {
        Method(_DSM, 4)
        {
            If (!Arg2) { Return (Buffer() { 0x03 } ) }
            Return (Package()
            {
                "QuietTimeAfterTyping", 0x0,
                "RM,deliverNotifications", ">n",
            })
        }
        
        Name (RMCF, Package ()
        {
            "Controller", 
            Package ()
            {
                "QuietTimeAfterTyping", 0x0,
                "RM,deliverNotifications", ">n"
            },
            "ALPS", 
            Package ()
            {
                "QuietTimeAfterTyping", 0x0,
                "RM,deliverNotifications", ">n"
            },
            "ALPS GlidePoint", 
            Package ()
            {
                "QuietTimeAfterTyping", 0x0,
                "RM,deliverNotifications", ">n"
            },
            "Trackpad", 
            Package ()
            {
                "QuietTimeAfterTyping", 0x0,
                "RM,deliverNotifications", ">n"
            },
            "Mouse", 
            Package ()
            {
                "QuietTimeAfterTyping", 0x0,
                "RM,deliverNotifications", ">n"
            }
        })
    }
}

