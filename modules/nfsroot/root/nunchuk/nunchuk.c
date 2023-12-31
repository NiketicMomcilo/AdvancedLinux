#include <linux/init.h>
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/input-polldev.h>

#define NUNCHUK_MODE_MIN	(1)
#define NUNCHUK_MODE_MAX	(2)

#define NUNCHUK_I2C_INIT_SIZE    (2)
#define NUNCHUK_I2C_POLL_SIZE    (6)
#define NUNCHUK_I2C_BUFFER_SIZE  (NUNCHUK_I2C_POLL_SIZE)
#define NUNCHUK_POLL_INTERVAL_MS (50)

#define NUNCHUK_AXES_INDEX_JOYSTICK_X	(0)
#define NUNCHUK_AXES_INDEX_JOYSTICK_Y	(1)
#define NUNCHUK_AXES_INDEX_ACC_X		(2)
#define NUNCHUK_AXES_INDEX_ACC_Y		(3)
#define NUNCHUK_AXES_INDEX_ACC_Z		(4)
#define NUNCHUK_AXES_INDEX_BUTTONS		(5)

#define NUNCHUK_AXES_INDEX_JOYSTICK_OFFSET	((s8) -128)
#define NUNCHUK_AXES_INDEX_ACC_OFFSET		((s8)    0)

#define NUNCHUK_AXES_ABSINFO_INDEX_ABS_X		(0)
#define NUNCHUK_AXES_ABSINFO_INDEX_ABS_Y		(1)
#define NUNCHUK_AXES_ABSINFO_INDEX_ABS_Z		(2)
#define NUNCHUK_AXES_ABSINFO_INDEX_ABS_RX		(3)
#define NUNCHUK_AXES_ABSINFO_INDEX_ABS_RY		(4)
#define NUNCHUK_AXES_ABSINFO_VALUE_DEFAULT		((s8)    0)	// latest reported value for the axis
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

static const struct input_absinfo axes_absinfo_default =
{
	.value		= NUNCHUK_AXES_ABSINFO_VALUE_DEFAULT,
	.minimum	= NUNCHUK_AXES_ABSINFO_MIN,
	.maximum	= NUNCHUK_AXES_ABSINFO_MAX,
	.fuzz		= NUNCHUK_AXES_ABSINFO_FUZZ,
	.flat		= NUNCHUK_AXES_ABSINFO_FLAT,
	.resolution	= NUNCHUK_AXES_ABSINFO_RESOLUTION,
};

struct nunchuk_inputs {
	int zpressed;
	int cpressed;
	s8 joystick_x;
	s8 joystick_y;
	s8 acc_x;
	s8 acc_y;
	s8 acc_z;
};

struct nunchuk_dev {
	struct input_polled_dev *polled_input;
	struct i2c_client *i2c_client;
	struct nunchuk_inputs inputs;
	u32 mode;
};

static int nunchuk_i2c_init(struct i2c_client *client)
{
	int status = 0;
	const char INIT_COMMAND[2][NUNCHUK_I2C_INIT_SIZE] = {{0xf0, 0x55}, {0xfb, 0x00}};

	do {
		// init nunchuk

		status = i2c_master_send(client, INIT_COMMAND[0], NUNCHUK_I2C_INIT_SIZE);
		if (status < 0)
			break;
		else
			status = 0;
		udelay(1000);

		status = i2c_master_send(client, INIT_COMMAND[1], NUNCHUK_I2C_INIT_SIZE);
		if (status < 0)
			break;
		else
			status = 0;
		mdelay(10);

	} while(0);

	return status;
}

static int nunchuk_i2c_get(struct nunchuk_dev *nunchuk)
{
	int status = 0;
	char read_cmd = 0x0;
	char buf[NUNCHUK_I2C_BUFFER_SIZE];

	do {
		status = i2c_master_send(nunchuk->i2c_client, &read_cmd, 1);
		if (status < 0)
			break;
		mdelay(10);

		status = i2c_master_recv(nunchuk->i2c_client, buf, NUNCHUK_I2C_POLL_SIZE);
		if (status < 0)
			break;
		mdelay(10);

		nunchuk->inputs.zpressed 	= ((buf[NUNCHUK_AXES_INDEX_BUTTONS] & BIT0) == 0) ? 1 : 0;
		nunchuk->inputs.cpressed 	= ((buf[NUNCHUK_AXES_INDEX_BUTTONS] & BIT1) == 0) ? 1 : 0;
		nunchuk->inputs.joystick_x	= buf[NUNCHUK_AXES_INDEX_JOYSTICK_X]	+ NUNCHUK_AXES_INDEX_JOYSTICK_OFFSET;
		nunchuk->inputs.joystick_y	= buf[NUNCHUK_AXES_INDEX_JOYSTICK_Y]	+ NUNCHUK_AXES_INDEX_JOYSTICK_OFFSET;
		nunchuk->inputs.acc_x		= buf[NUNCHUK_AXES_INDEX_ACC_X]			+ NUNCHUK_AXES_INDEX_ACC_OFFSET;
		nunchuk->inputs.acc_y		= buf[NUNCHUK_AXES_INDEX_ACC_Y]			+ NUNCHUK_AXES_INDEX_ACC_OFFSET;
		nunchuk->inputs.acc_z		= buf[NUNCHUK_AXES_INDEX_ACC_Z]			+ NUNCHUK_AXES_INDEX_ACC_OFFSET;

	} while(0);

	return status;
}

static void poll_buttons(struct nunchuk_dev *nunchuk)
{
	input_event(nunchuk->polled_input->input, EV_KEY, BTN_Z, nunchuk->inputs.zpressed);
	input_event(nunchuk->polled_input->input, EV_KEY, BTN_C, nunchuk->inputs.cpressed);
}

static void poll_axes(struct nunchuk_dev *nunchuk)
{
	input_event(nunchuk->polled_input->input, EV_ABS, ABS_RX, nunchuk->inputs.joystick_x);
	input_event(nunchuk->polled_input->input, EV_ABS, ABS_RY, nunchuk->inputs.joystick_y);
	input_event(nunchuk->polled_input->input, EV_ABS, ABS_X, nunchuk->inputs.acc_x);
	input_event(nunchuk->polled_input->input, EV_ABS, ABS_Y, nunchuk->inputs.acc_y);
	input_event(nunchuk->polled_input->input, EV_ABS, ABS_Z, nunchuk->inputs.acc_z);
}

static void nunchuk_poll(struct input_polled_dev *polled_input)
{
	int status = 0;
	struct nunchuk_dev *nunchuk = polled_input->private;

	do {
		status = nunchuk_i2c_get(nunchuk);

		if (status < 0) {
			dev_err(&nunchuk->i2c_client->dev, "Unable to poll device.\n");
			status = nunchuk_i2c_init(nunchuk->i2c_client);
			break;
		}

		switch(nunchuk->mode) {
		default:
		case 2:
			poll_axes(nunchuk);

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wimplicit-fallthrough"
			nop(); // deliberate fall through, because case 2 contains case 1 as a subset
#pragma GCC diagnostic pop

		case 1:
			poll_buttons(nunchuk);

			break;
		}

		input_sync(polled_input->input);

	} while(0);
}

static int nunchuk_probe(struct i2c_client *client, const struct i2c_device_id *id_table)
{
	int status = 0;
	struct input_polled_dev *polled_input = NULL;
	struct input_dev *input = NULL;
	struct nunchuk_dev *nunchuk = NULL;
	struct input_absinfo *p_absinfo = NULL;

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
	status = of_property_read_u32(client->dev.of_node, "mode", &nunchuk->mode);
	if (status != 0) {
		dev_err(&client->dev, "Cannot access mode property.\n");
		goto fail_dts_property;
	} else if ((nunchuk->mode < NUNCHUK_MODE_MIN) || (nunchuk->mode > NUNCHUK_MODE_MAX)) {
		dev_err(&client->dev, "Invalid mode property value.\n");
		status = -EINVAL;
		goto fail_dts_property;
	}

	polled_input->private = nunchuk;
	polled_input->poll = nunchuk_poll;
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

	status = nunchuk_i2c_get(nunchuk);
	if (status < 0) {
		dev_err(&client->dev, "Failed to read Nunchuk inputs.\n");
		goto fail_i2c_inputs;
	}

	switch(nunchuk->mode) {
	default:
	case 2:
		// Axis events
		input_alloc_absinfo(input);

		p_absinfo = &input->absinfo[NUNCHUK_AXES_ABSINFO_INDEX_ABS_X];
		*p_absinfo = axes_absinfo_default;
		p_absinfo->value = nunchuk->inputs.acc_x;

		p_absinfo = &input->absinfo[NUNCHUK_AXES_ABSINFO_INDEX_ABS_Y];
		*p_absinfo = axes_absinfo_default;
		p_absinfo->value = nunchuk->inputs.acc_y;

		p_absinfo = &input->absinfo[NUNCHUK_AXES_ABSINFO_INDEX_ABS_Z];
		*p_absinfo = axes_absinfo_default;
		p_absinfo->value = nunchuk->inputs.acc_z;

		p_absinfo = &input->absinfo[NUNCHUK_AXES_ABSINFO_INDEX_ABS_RX];
		*p_absinfo = axes_absinfo_default;
		p_absinfo->value = nunchuk->inputs.joystick_x;

		p_absinfo = &input->absinfo[NUNCHUK_AXES_ABSINFO_INDEX_ABS_RY];
		*p_absinfo = axes_absinfo_default;
		p_absinfo->value = nunchuk->inputs.joystick_y;

		set_bit(EV_ABS, input->evbit);
		set_bit(ABS_X, input->absbit);
		set_bit(ABS_Y, input->absbit);
		set_bit(ABS_Z, input->absbit);
		set_bit(ABS_RX, input->absbit);
		set_bit(ABS_RY, input->absbit);

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wimplicit-fallthrough"
		nop(); // deliberate fall through, because case 2 contains case 1 as a subset
#pragma GCC diagnostic pop

	case 1:

		// Button events
		set_bit(EV_KEY, input->evbit);
		set_bit(BTN_C, input->keybit);
		set_bit(BTN_Z, input->keybit);

		break;
	}

	status = input_register_polled_device(polled_input);
	if (status < 0) {
		dev_err(&client->dev, "Failed to register input polled device.\n");
		goto fail_input_register;
	}

	return status;

fail_input_register:
	// NOP

fail_i2c_inputs:
	// NOP

fail_i2c_init:
	// NOP

fail_dts_property:
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

