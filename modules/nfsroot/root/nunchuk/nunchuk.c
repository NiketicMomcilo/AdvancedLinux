#include <linux/init.h>
#include <linux/module.h>
#include <linux/i2c.h>

static int nunchuk_probe(struct i2c_client *client, const struct i2c_device_id *id_table)
{
	pr_info("nunchuk_probe()\n");

	return 0;
}

static int nunchuk_remove(struct i2c_client *client)
{
	pr_info("nunchuk_remove()\n");

	return 0;
}

static struct of_device_id nunchuk_dt_match[] = {
        { .compatible = "nintendo,nunchuk" },
        { },
};

static struct i2c_driver nunchuk_driver = {
        .driver = {
                .name = "nunchuk",
                .owner = THIS_MODULE,
                .of_match_table = of_match_ptr(nunchuk_dt_match),
        },
        .probe = nunchuk_probe,
        .remove = nunchuk_remove,
};

module_i2c_driver(nunchuk_driver);

MODULE_LICENSE("GPL");

