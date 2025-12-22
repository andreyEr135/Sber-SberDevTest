#ifndef devscanH
#define devscanH
//---------------------------------------------------------------------------

#define TBL_ID       0

//---------------------------------------------------------------------------

#define PCI_TBL            "/PCI_TBL"
#define PCI_COUNT         11
#define PCI_BUS            1
#define PCI_BUSSTR         2
#define PCI_VENDEV         3
#define PCI_VENDEVSTR      4
#define PCI_GEN            5
#define PCI_X              6
#define PCI_CLASS          7
#define PCI_NAME           8
#define PCI_SYSPATH        9
#define PCI_PARENT        10
#define PCI_DRIVER        11
//---------------------------------------------------------------------------

#define USB_TBL            "/USB_TBL"
#define USB_COUNT         10
#define USB_BUSPORT        1
#define USB_DEVNUM         2
#define USB_VENDEV         3
#define USB_VENDEVSTR      4
#define USB_BCD            5
#define USB_SPEED          6
#define USB_NAME           7
#define USB_SERIAL         8
#define USB_SYSPATH        9
#define USB_DRIVER        10
//---------------------------------------------------------------------------

#define BLK_TBL          "/BLK_TBL"
#define BLK_COUNT        7
#define BLK_DEV          1
#define BLK_NAME         2
#define BLK_SERIAL       3
#define BLK_SIZE         4
#define BLK_MOUNT        5
#define BLK_SYSPATH      6
#define BLK_DRIVER       7

//---------------------------------------------------------------------------

#define NET_TBL            "/NET_TBL"
#define NET_COUNT          5
#define NET_NAME           1
#define NET_TYPE           2
#define NET_SPEED          3
#define NET_SYSPATH        4
#define NET_DRIVER         5
//---------------------------------------------------------------------------

#define SENSORDEV_TBL       "/SENSORDEV_TBL"
#define SENSORDEV_COUNT     1
#define SENSORDEV_NAME      1
//---------------------------------------------------------------------------

#define SENSOR_TBL          "/SENSOR_TBL"
#define SENSOR_COUNT        7
#define SENSOR_DEVID        1
#define SENSOR_TYPE         2
#define SENSOR_NAME         3
#define SENSOR_VALUE        4
#define SENSOR_FLAG         5
#define SENSOR_TIME         6
#define SENSOR_WIDTH        7
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
#endif

