/**
 * @file country_region.h
 * @brief Named constants for the CPS3 region DIP-switch value (Country).
 *
 * The global `Country` (u8) is set at boot from the board's region setting
 * and is read-only during gameplay. These constants replace bare integer
 * comparisons throughout the codebase.
 */
#ifndef COUNTRY_REGION_H
#define COUNTRY_REGION_H

#define COUNTRY_JAPAN   1
#define COUNTRY_USA     2
#define COUNTRY_ASIA    3
#define COUNTRY_EUROPE  4
#define COUNTRY_KOREA   8

#endif /* COUNTRY_REGION_H */
