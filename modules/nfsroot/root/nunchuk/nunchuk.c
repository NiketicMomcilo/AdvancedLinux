#include <linux/init.h>
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/input-polldev.h>

#define NUNCHUK_I2C_BUFFER_SIZE  (10)
#define NUNCHUK_POLL_INTERVAL_MS (50)

#define BIT0	0x00000001
#define BIT1	0x00000002
#define BIT2	0x00000004
#define BIT3	0x00000008
#define BIT4	0x00000010
#define BIT5	0x00000020
#define BIT6	0x00000040
#define BIT7	0x00000080

struct nunchuk_dev {
	struct input_polled_dev *polled_input;
	struct i2c_client *i2c_client;
};

static int nunchuk_init(struct i2c_client *client)
{
	int status = 0;
	char buf[NUNCHUK_I2C_BUFFER_SIZE] = { 0x0 };

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

	} while(0);

	return status;
}

static void nunchuk_poll(struct input_polled_dev *polled_input)
{
	char buf[NUNCHUK_I2C_BUFFER_SIZE] = { 0x0 };
	int zpressed = 0;
	int cpressed = 0;
	static int zpressed_prev = 0; // TODO: Remove
	static int cpressed_prev = 0; // TODO: Remove

	struct nunchuk_dev *nunchuk = polled_input->private;

	// read nunchuk register

	buf[0] = 0x0;
	(void)i2c_master_send(nunchuk->i2c_client, buf, 1);
	mdelay(10);

	(void)i2c_master_recv(nunchuk->i2c_client, buf, 6);
	mdelay(10);


	zpressed = ((buf[5] & BIT0) == 0) ? 1 : 0;
	cpressed = ((buf[5] & BIT1) == 0) ? 1 : 0;

	if (zpressed != zpressed_prev) {
		if(zpressed == 1)
			pr_info("Z key is pressed.\n");
		else
			pr_info("Z key is NOT pressed.\n");
	}

	if (cpressed != cpressed_prev) {
		if(cpressed == 1)
			pr_info("C key is pressed.\n");
		else
			pr_info("C key is NOT pressed.\n");
	}

	zpressed_prev = zpressed;
	cpressed_prev = cpressed;

	input_event(polled_input->input, EV_KEY, BTN_Z, zpressed);
	input_event(polled_input->input, EV_KEY, BTN_C, cpressed);
	input_sync(polled_input->input);
}

static int nunchuk_probe(struct i2c_client *client, const struct i2c_device_id *id_table)
{
	int status = 0;
	struct input_polled_dev *polled_input = NULL;
	struct input_dev *input = NULL;
	struct nunchuk_dev *nunchuk = NULL;

	pr_info("nunchuk_probe()\n"); // TODO: Remove

	do {
		polled_input = input_allocate_polled_device();
		if (polled_input == NULL) {
			// TODO: Add error message
			status = -ENOMEM;
			break;
		}

		nunchuk = devm_kzalloc(&client->dev, sizeof(*nunchuk), GFP_KERNEL);
		if (nunchuk == NULL) {
			// TODO: Add error message
			return -ENOMEM;
		}

		nunchuk->i2c_client = client;
		nunchuk->polled_input = polled_input;
		polled_input->private = nunchuk;
		polled_input->poll = nunchuk_poll;
		polled_input->poll_interval = NUNCHUK_POLL_INTERVAL_MS;
		i2c_set_clientdata(client, nunchuk);

		input = polled_input->input;
		input->dev.parent = &client->dev;
		input->name = "Wii Nunchuk";
		input->id.bustype = BUS_I2C;

		set_bit(EV_KEY, input->evbit);
		set_bit(BTN_C, input->keybit);
		set_bit(BTN_Z, input->keybit);

		status = input_register_polled_device(polled_input);
		if (status < 0) {
			// TODO: Add error message
			break;
		}

		status = nunchuk_init(client);
		if (status < 0) {
			// TODO: Add error message
			break;
		}
	} while(0);

	return status;
}

static int nunchuk_remove(struct i2c_client *client)
{
	int status = 0;
	struct nunchuk_dev *nunchuk = NULL;

	pr_info("nunchuk_remove()\n"); // TODO: Remove

	nunchuk = i2c_get_clientdata(client);

	do {
		if (nunchuk == NULL) {
			// TODO: Add error message
			status = -EBUSY;
			break;
		}

		input_unregister_polled_device(nunchuk->polled_input);
		input_free_polled_device(nunchuk->polled_input);
	} while(0);

	return status;
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

