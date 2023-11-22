#include <linux/init.h>
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/input-polldev.h>

#define NUNCHUK_I2C_BUFFER_SIZE  (10)
#define NUNCHUK_POLL_INTERVAL_MS (50)

#define NUNCHUK_AXES_INDEX_JOYSTICK_X	(0)
#define NUNCHUK_AXES_INDEX_JOYSTICK_Y	(1)
#define NUNCHUK_AXES_INDEX_ACC_X		(2)
#define NUNCHUK_AXES_INDEX_ACC_Y		(3)
#define NUNCHUK_AXES_INDEX_ACC_Z		(4)
#define NUNCHUK_AXES_INDEX_BUTTONS		(5)

#define NUNCHUK_AXES_INDEX_JOYSTICK_OFFSET	((s8) -128)
#define NUNCHUK_AXES_INDEX_ACC_OFFSET		((s8)    0)

#define NUNCHUK_AXES_ABSINFO_NUMBER_OF_AXES		(5)
#define NUNCHUK_AXES_ABSINFO_INDEX_ABS_X		(0)
#define NUNCHUK_AXES_ABSINFO_INDEX_ABS_Y		(1)
#define NUNCHUK_AXES_ABSINFO_INDEX_ABS_Z		(2)
#define NUNCHUK_AXES_ABSINFO_INDEX_ABS_RX		(3)
#define NUNCHUK_AXES_ABSINFO_INDEX_ABS_RY		(4)
#define NUNCHUK_AXES_ABSINFO_INIT_VALUE			((s8)    0)	// latest reported value for the axis
#define NUNCHUK_AXES_ABSINFO_MIN				((s8) -128) // specifies minimum value for the axis
#define NUNCHUK_AXES_ABSINFO_MAX				((s8)  127) // specifies maximum value for the axis
#define NUNCHUK_AXES_ABSINFO_FUZZ				((s8)   10) // specifies fuzz value that is used to filter noise
															// from the event stream
#define NUNCHUK_AXES_ABSINFO_FLAT				((s8)    0) // values that are within this value will be discarded by
															// joydev interface and reported as 0 instead
#define NUNCHUK_AXES_ABSINFO_RESOLUTION			((s8)    1) // specifies resolution for the values reported for the axis

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

static void nunchuk_i2c_get(struct i2c_client *client, char *buf)
{
	char read_cmd = 0x0;

	(void)i2c_master_send(client, &read_cmd, 1);
	mdelay(10);

	(void)i2c_master_recv(client, buf, 6);
	mdelay(10);

	buf[NUNCHUK_AXES_INDEX_JOYSTICK_X] += NUNCHUK_AXES_INDEX_JOYSTICK_OFFSET;
	buf[NUNCHUK_AXES_INDEX_JOYSTICK_Y] += NUNCHUK_AXES_INDEX_JOYSTICK_OFFSET;
	buf[NUNCHUK_AXES_INDEX_ACC_X] += NUNCHUK_AXES_INDEX_ACC_OFFSET;
	buf[NUNCHUK_AXES_INDEX_ACC_Y] += NUNCHUK_AXES_INDEX_ACC_OFFSET;
	buf[NUNCHUK_AXES_INDEX_ACC_Z] += NUNCHUK_AXES_INDEX_ACC_OFFSET;
}

#if (DEBUG_POLL_FUNCTION == 1)

static void nunchuk_poll_axes(struct input_polled_dev *polled_input)
{
	char buf[NUNCHUK_I2C_BUFFER_SIZE] = { 0x0 };
	s8 joystick_x = 0;
	s8 joystick_y = 0;
	s8 acc_x = 0;
	s8 acc_y = 0;
	s8 acc_z = 0;

	struct nunchuk_dev *nunchuk = polled_input->private;

	nunchuk_i2c_get(nunchuk->i2c_client, buf);

	joystick_x	= buf[NUNCHUK_AXES_INDEX_JOYSTICK_X];
	joystick_y	= buf[NUNCHUK_AXES_INDEX_JOYSTICK_Y];
	acc_x		= buf[NUNCHUK_AXES_INDEX_ACC_X];
	acc_y		= buf[NUNCHUK_AXES_INDEX_ACC_Y];
	acc_z		= buf[NUNCHUK_AXES_INDEX_ACC_Z];

	input_event(polled_input->input, EV_ABS, ABS_RX, joystick_x);
	input_event(polled_input->input, EV_ABS, ABS_RY, joystick_y);
	input_event(polled_input->input, EV_ABS, ABS_X, acc_x);
	input_event(polled_input->input, EV_ABS, ABS_Y, acc_y);
	input_event(polled_input->input, EV_ABS, ABS_Z, acc_z);
	input_sync(polled_input->input);
}

#else

static void nunchuk_poll_buttons(struct input_polled_dev *polled_input)
{
	char buf[NUNCHUK_I2C_BUFFER_SIZE] = { 0x0 };
	int zpressed = 0;
	int cpressed = 0;

	struct nunchuk_dev *nunchuk = polled_input->private;

	nunchuk_i2c_get(nunchuk->i2c_client, buf);

	zpressed = ((buf[NUNCHUK_AXES_INDEX_BUTTONS] & BIT0) == 0) ? 1 : 0;
	cpressed = ((buf[NUNCHUK_AXES_INDEX_BUTTONS] & BIT1) == 0) ? 1 : 0;

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
#if (DEBUG_POLL_FUNCTION == 1)
	char buf[NUNCHUK_I2C_BUFFER_SIZE] = { 0x0 };
#endif

	polled_input = devm_input_allocate_polled_device(&client->dev);
	if (polled_input == NULL) {
		dev_err(&client->dev, "Failed to allocate polled device.\n");
		status = -ENOMEM;
		goto fail_input_alloc;
	}

	nunchuk = devm_kzalloc(&client->dev, sizeof(*nunchuk), GFP_KERNEL);
	if (nunchuk == NULL) {
		dev_err(&client->dev, "Failed to allocate device memory.\n");
		status = -ENOMEM;
		goto fail_mem_alloc;
	}

	nunchuk->i2c_client = client;
	nunchuk->polled_input = polled_input;
	polled_input->private = nunchuk;
#if (DEBUG_POLL_FUNCTION == 1)
	polled_input->poll = nunchuk_poll_axes;
#else
	polled_input->poll = nunchuk_poll_buttons;
#endif
	polled_input->poll_interval = NUNCHUK_POLL_INTERVAL_MS;
	i2c_set_clientdata(client, nunchuk);

	status = nunchuk_i2c_init(client);
	if (status < 0) {
		dev_err(&client->dev, "Failed Nunchuk I2C initialization.\n");
		goto fail_i2c_init;
	}

	input = polled_input->input;
	input->dev.parent = &client->dev;
	input->name = "Wii Nunchuk";
	input->id.bustype = BUS_I2C;

#if (DEBUG_POLL_FUNCTION == 1)
	input->absinfo = devm_kzalloc(&client->dev, (NUNCHUK_AXES_ABSINFO_NUMBER_OF_AXES * sizeof(*input->absinfo)), GFP_KERNEL);
	if (input->absinfo == NULL) {
		dev_err(&client->dev, "Failed to allocate input absinfo memory.\n");
		status = -ENOMEM;
		goto fail_absinfo_alloc;
	}

	nunchuk_i2c_get(nunchuk->i2c_client, buf);

	(input->absinfo + NUNCHUK_AXES_ABSINFO_INDEX_ABS_X)->value		= buf[NUNCHUK_AXES_INDEX_ACC_X];
	(input->absinfo + NUNCHUK_AXES_ABSINFO_INDEX_ABS_X)->minimum	= NUNCHUK_AXES_ABSINFO_MIN;
	(input->absinfo + NUNCHUK_AXES_ABSINFO_INDEX_ABS_X)->maximum	= NUNCHUK_AXES_ABSINFO_MAX;
	(input->absinfo + NUNCHUK_AXES_ABSINFO_INDEX_ABS_X)->fuzz		= NUNCHUK_AXES_ABSINFO_FUZZ;
	(input->absinfo + NUNCHUK_AXES_ABSINFO_INDEX_ABS_X)->flat		= NUNCHUK_AXES_ABSINFO_FLAT;
	(input->absinfo + NUNCHUK_AXES_ABSINFO_INDEX_ABS_X)->resolution	= NUNCHUK_AXES_ABSINFO_RESOLUTION;

	(input->absinfo + NUNCHUK_AXES_ABSINFO_INDEX_ABS_Y)->value		= buf[NUNCHUK_AXES_INDEX_ACC_Y];
	(input->absinfo + NUNCHUK_AXES_ABSINFO_INDEX_ABS_Y)->minimum	= NUNCHUK_AXES_ABSINFO_MIN;
	(input->absinfo + NUNCHUK_AXES_ABSINFO_INDEX_ABS_Y)->maximum	= NUNCHUK_AXES_ABSINFO_MAX;
	(input->absinfo + NUNCHUK_AXES_ABSINFO_INDEX_ABS_Y)->fuzz		= NUNCHUK_AXES_ABSINFO_FUZZ;
	(input->absinfo + NUNCHUK_AXES_ABSINFO_INDEX_ABS_Y)->flat		= NUNCHUK_AXES_ABSINFO_FLAT;
	(input->absinfo + NUNCHUK_AXES_ABSINFO_INDEX_ABS_Y)->resolution	= NUNCHUK_AXES_ABSINFO_RESOLUTION;

	(input->absinfo + NUNCHUK_AXES_ABSINFO_INDEX_ABS_Z)->value		= buf[NUNCHUK_AXES_INDEX_ACC_Z];
	(input->absinfo + NUNCHUK_AXES_ABSINFO_INDEX_ABS_Z)->minimum	= NUNCHUK_AXES_ABSINFO_MIN;
	(input->absinfo + NUNCHUK_AXES_ABSINFO_INDEX_ABS_Z)->maximum	= NUNCHUK_AXES_ABSINFO_MAX;
	(input->absinfo + NUNCHUK_AXES_ABSINFO_INDEX_ABS_Z)->fuzz		= NUNCHUK_AXES_ABSINFO_FUZZ;
	(input->absinfo + NUNCHUK_AXES_ABSINFO_INDEX_ABS_Z)->flat		= NUNCHUK_AXES_ABSINFO_FLAT;
	(input->absinfo + NUNCHUK_AXES_ABSINFO_INDEX_ABS_Z)->resolution	= NUNCHUK_AXES_ABSINFO_RESOLUTION;

	(input->absinfo + NUNCHUK_AXES_ABSINFO_INDEX_ABS_RX)->value			= buf[NUNCHUK_AXES_INDEX_JOYSTICK_X];
	(input->absinfo + NUNCHUK_AXES_ABSINFO_INDEX_ABS_RX)->minimum		= NUNCHUK_AXES_ABSINFO_MIN;
	(input->absinfo + NUNCHUK_AXES_ABSINFO_INDEX_ABS_RX)->maximum		= NUNCHUK_AXES_ABSINFO_MAX;
	(input->absinfo + NUNCHUK_AXES_ABSINFO_INDEX_ABS_RX)->fuzz			= NUNCHUK_AXES_ABSINFO_FUZZ;
	(input->absinfo + NUNCHUK_AXES_ABSINFO_INDEX_ABS_RX)->flat			= NUNCHUK_AXES_ABSINFO_FLAT;
	(input->absinfo + NUNCHUK_AXES_ABSINFO_INDEX_ABS_RX)->resolution	= NUNCHUK_AXES_ABSINFO_RESOLUTION;

	(input->absinfo + NUNCHUK_AXES_ABSINFO_INDEX_ABS_RY)->value			= buf[NUNCHUK_AXES_INDEX_JOYSTICK_Y];
	(input->absinfo + NUNCHUK_AXES_ABSINFO_INDEX_ABS_RY)->minimum		= NUNCHUK_AXES_ABSINFO_MIN;
	(input->absinfo + NUNCHUK_AXES_ABSINFO_INDEX_ABS_RY)->maximum		= NUNCHUK_AXES_ABSINFO_MAX;
	(input->absinfo + NUNCHUK_AXES_ABSINFO_INDEX_ABS_RY)->fuzz			= NUNCHUK_AXES_ABSINFO_FUZZ;
	(input->absinfo + NUNCHUK_AXES_ABSINFO_INDEX_ABS_RY)->flat			= NUNCHUK_AXES_ABSINFO_FLAT;
	(input->absinfo + NUNCHUK_AXES_ABSINFO_INDEX_ABS_RY)->resolution	= NUNCHUK_AXES_ABSINFO_RESOLUTION;

	set_bit(EV_ABS, input->evbit);
	set_bit(ABS_X, input->absbit);
	set_bit(ABS_Y, input->absbit);
	set_bit(ABS_Z, input->absbit);
	set_bit(ABS_RX, input->absbit);
	set_bit(ABS_RY, input->absbit);
#else
	set_bit(EV_KEY, input->evbit);
	set_bit(BTN_C, input->keybit);
	set_bit(BTN_Z, input->keybit);
#endif

	status = input_register_polled_device(polled_input);
	if (status < 0) {
		dev_err(&client->dev, "Failed to register input polled device.\n");
		goto fail_input_register;
	}

	return status;


fail_input_register:
	// NOP

fail_i2c_init:
	// NOP

fail_absinfo_alloc:
	// NOP

fail_mem_alloc:
	// NOP

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

