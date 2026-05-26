#ifndef GRETH_HW_CFG_H
#define GRETH_HW_CFG_H

/**
 * @brief GRETH hardware configuration for lwIP initialization
 *
 * This structure is used to pass hardware-specific configuration parameters to
 * the GRETH initialization function. It includes the device ID for driver
 * attachment and the PHY address for the PHY connected to the GRETH device.
 *
 * This structure is passed to the driver at initialization by assigning it
 * to the `state` field of the lwIP `netif` struct.
 *
 * @param greth_id The unique identifier for the GRETH device, used for driver
 * attachment.
 * @param phy_addr The address of the PHY device on the MDIO bus, used for
 * driver attachment and PHY configuration. If set to zero, the driver will
 * attempt to auto-detect the PHY address.
 *
 */
struct lwip_greth_hw_cfg {
  unsigned int greth_id;
  unsigned int phy_addr;
};

#endif