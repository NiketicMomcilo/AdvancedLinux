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

