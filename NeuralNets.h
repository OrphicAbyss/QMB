typedef struct neuralnetwork_s {
	float *inputs;			//input values
	float *inputWeights;	//number input weights = (inputs + 1) * inputs
	float *hiddens;			//values for input to hidden units
	float *hiddenWeights;	//used in a 2 layer network (second layers weights) = (hidden + 1) * hidden
	float *hiddenError;
	float *outputs;			//output values, gained from running the inputs through the network
	float *expectedOutputs;	//expected output values, used in learning
	float learningRate;		//rate that learning should effect the network weights
	int numOfInputs;
	int numOfHiddens;
	int numOfOutputs;
	int free;

} neuralnetwork_t;

#define DELTA_RULE 1

//defualt to 128 networks (a quarter of max) 128 is alot of networks anyway
//lots of computations in NN's
#define MAX_NEURAL_NETWORKS 512

void NN_init();
void NN_deinit();
int NN_add(int inputs, int hidden, int outputs);
void NN_remove(int ident);
void NN_setInputs(int ident, int start, int n, float *inputs);
void NN_setInputN(int ident, int n, float input);
void NN_run(int ident);
float NN_getOutputN(int ident, int n);
void NN_getOutputs(int ident, int start, int n, float *outputs);
void NN_setExpectedOutputs(int ident, int n, float expected);
void NN_learn(int ident, int type);
void NN_setLearningRate(int ident, float rate);
