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

#define DEBUG_POLL_FUNCTION (1)

struct nunchuk_dev {
	struct input_polled_dev *polled_input;
	struct i2c_client *i2c_client;
};

static int nunchuk_i2c_init(struct i2c_client *client)
{
	int status = 0;
	char buf[NUNCHUK_I2C_BUFFER_SIZE] = { 0x0 };

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

#if (DEBUG_POLL_FUNCTION == 1)

static void nunchuk_poll_accelerometer(struct input_polled_dev *polled_input)
{
	char buf[NUNCHUK_I2C_BUFFER_SIZE] = { 0x0 };
	u8 acc_x = 0;
	u8 acc_y = 0;
	u8 acc_z = 0;

	struct nunchuk_dev *nunchuk = polled_input->private;

	// read nunchuk register

	buf[0] = 0x0;
	(void)i2c_master_send(nunchuk->i2c_client, buf, 1);
	mdelay(10);

	(void)i2c_master_recv(nunchuk->i2c_client, buf, 6);
	mdelay(10);


	acc_x = buf[2];
	acc_y = buf[3];
	acc_z = buf[4];

	pr_info("[DEBUG] (x, y, z) = (%u, %u, %u)\n", acc_x, acc_y, acc_z);
}

#else

static void nunchuk_poll_buttons(struct input_polled_dev *polled_input)
{
	char buf[NUNCHUK_I2C_BUFFER_SIZE] = { 0x0 };
	int zpressed = 0;
	int cpressed = 0;

	struct nunchuk_dev *nunchuk = polled_input->private;

	// read nunchuk register

	buf[0] = 0x0;
	(void)i2c_master_send(nunchuk->i2c_client, buf, 1);
	mdelay(10);

	(void)i2c_master_recv(nunchuk->i2c_client, buf, 6);
	mdelay(10);


	zpressed = ((buf[5] & BIT0) == 0) ? 1 : 0;
	cpressed = ((buf[5] & BIT1) == 0) ? 1 : 0;

	input_event(polled_input->input, EV_KEY, BTN_Z, zpressed);
	input_event(polled_input->input, EV_KEY, BTN_C, cpressed);
	input_sync(polled_input->input);
}

#endif

static int nunchuk_probe(struct i2c_client *client, const struct i2c_device_id *id_table)
{
	int status = 0;
	struct input_polled_dev *polled_input = NULL;
	struct input_dev *input = NULL;
	struct nunchuk_dev *nunchuk = NULL;

	polled_input = devm_input_allocate_polled_device(&client->dev);
	if (polled_input == NULL) {
		dev_err(&client->dev, "Failed to allocate polled device.\n");
		status = -ENOMEM;
		goto fail_input_alloc;
	}

	nunchuk = devm_kzalloc(&client->dev, sizeof(*nunchuk), GFP_KERNEL);
	if (nunchuk == NULL) {
		dev_err(&client->dev, "Failed to allocate memory.\n");
		input_free_polled_device(polled_input);
		status = -ENOMEM;
		goto fail_mem_alloc;
	}

	nunchuk->i2c_client = client;
	nunchuk->polled_input = polled_input;
	polled_input->private = nunchuk;
#if (DEBUG_POLL_FUNCTION == 1)
	polled_input->poll = nunchuk_poll_accelerometer;
#else
	polled_input->poll = nunchuk_poll_buttons;
#endif
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
		dev_err(&client->dev, "Failed to register input polled device.\n");
		goto fail_input_register;
	}

	status = nunchuk_i2c_init(client);
	if (status < 0) {
		dev_err(&client->dev, "Failed Nunchuk I2C initialization.\n");
		goto fail_i2c_init;
	}

	return status;


fail_i2c_init:
	input_unregister_polled_device(nunchuk->polled_input);

fail_input_register:
	// NOP

fail_mem_alloc:
	input_free_polled_device(nunchuk->polled_input);

fail_input_alloc:
	// NOP

	return status;
}

static int nunchuk_remove(struct i2c_client *client)
{
	int status = 0;
	struct nunchuk_dev *nunchuk = NULL;

	nunchuk = i2c_get_clientdata(client);
	if (nunchuk == NULL) {
		dev_err(&client->dev, "Unable to get device data.\n");
		status = -EBUSY;
		goto fail_device_data;
	}

	input_unregister_polled_device(nunchuk->polled_input);

	return status;


fail_device_data:
	// NOP

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

