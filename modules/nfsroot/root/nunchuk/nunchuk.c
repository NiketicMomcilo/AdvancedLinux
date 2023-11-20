#include <linux/init.h>
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/delay.h>

#define BIT0	0x00000001
#define BIT1	0x00000002
#define BIT2	0x00000004
#define BIT3	0x00000008
#define BIT4	0x00000010
#define BIT5	0x00000020
#define BIT6	0x00000040
#define BIT7	0x00000080

static int nunchuck_init(struct i2c_client *client)
{
	char buf[8] = { 0x0 };
	int status = 0;

	pr_info("nunchuk_init()\n");

	do {
		// init nunchuk

		buf[0] = 0xf0;
		buf[1] = 0x55;
		status = i2c_master_send(client, buf, 2);
		if (status < 0)
			break;
		else
			status = 0;
		udelay(1000);

		buf[0] = 0xfb;
		buf[1] = 0x0;
		status = i2c_master_send(client, buf, 2);
		if (status < 0)
			break;
		else
			status = 0;
		mdelay(10);

		// read nunchuk register

		buf[0] = 0x0;
		buf[1] = 0x0;
		status = i2c_master_send(client, buf, 1);
		if (status < 0)
			break;
		else
			status = 0;
		mdelay(10);

		status = i2c_master_recv(client, buf, 6);
		if (status < 0)
			break;
		else
			status = 0;
		mdelay(10);

	} while(0);

	return status;
}


static int nunchuk_probe(struct i2c_client *client, const struct i2c_device_id *id_table)
{
	char buf[10] = { 0x0 };
	int status = 0;
	int zpressed = 0;
	int cpressed = 0;

	pr_info("nunchuk_probe()\n");

	do {
		// init nunchuk10
		status = nunchuck_init(client);

		// TODO: Remove - BEGIN (not needed for polling)

		buf[0] = 0x0;
		buf[1] = 0x0;
		status = i2c_master_send(client, buf, 1);
		if (status < 0)
			break;
		else
			status = 0;
		mdelay(10);

		status = i2c_master_recv(client, buf, 6);
		if (status < 0)
			break;
		else
			status = 0;
		// TODO: Remove - END

		zpressed = ((buf[5] & BIT0) == 0) ? 1 : 0;
		cpressed = ((buf[5] & BIT1) == 0) ? 1 : 0;

		if(zpressed == 1)
			pr_info("Z key is pressed.\n");
		else
			pr_info("Z key is NOT pressed.\n");

		if(cpressed == 1)
			pr_info("C key is pressed.\n");
		else
			pr_info("C key is NOT pressed.\n");

	} while(0);

	return status;
}

static int nunchuk_remove(struct i2c_client *client)
{
	pr_info("nunchuk_remove()\n");

	return 0;
}


static const struct i2c_device_id nunchuk_id[] = {
	{ "nunchuk", 0 },
	{ },
};
MODULE_DEVICE_TABLE(i2c, nunchuk_id);

#ifdef CONFIG_OF

static const struct of_device_id nunchuk_dt_ids[] = {
	{ .compatible = "nintendo,nunchuk" },
	{ },
};
MODULE_DEVICE_TABLE(of, nunchuk_dt_ids);

#endif

static struct i2c_driver nunchuk_driver = {
	.id_table = nunchuk_id,
	.driver = {
		.name = "nunchuk",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(nunchuk_dt_ids),
	},
	.probe = nunchuk_probe,
	.remove = nunchuk_remove,
};

module_i2c_driver(nunchuk_driver);

MODULE_LICENSE("GPL");

