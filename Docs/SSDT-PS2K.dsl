/*
 * Intel ACPI Component Architecture
 * AML/ASL+ Disassembler version 20200925 (64-bit version)
 * Copyright (c) 2000 - 2020 Intel Corporation
 * 
 * Disassembling to symbolic ASL+ operators
 *
 * Disassembly of iASLDWj0QW.aml, Sat Feb  5 21:18:09 2022
 *
 * Original Table Header:
 *     Signature        "SSDT"
 *     Length           0x000000C2 (194)
 *     Revision         0x02
 *     Checksum         0x0B
 *     OEM ID           "ACDT"
 *     OEM Table ID     "PS2K"
 *     OEM Revision     0x00000000 (0)
 *     Compiler ID      "INTL"
 *     Compiler Version 0x20200925 (538970405)
 */
DefinitionBlock ("", "SSDT", 2, "ACDT", "PS2K", 0x00000000)
{
    External (_SB_.PCI0.LPCB.PS2K, DeviceObj)

    Name (_SB.PCI0.LPCB.PS2K.RMCF, Package (0x02)
    {
        "Keyboard", 
        Package (0x04)
        {
            "Swap command and option", 
            ">n", 
            "Custom ADB Map", 
            Package (0x06)
            {
                Package (0x00){}, 
                "1d=3b", 
                "e05b=37", 
                "38=3a", 
                "e038=3d", 
                "e01d=3e"
            }
        }
    })
}

