/*********************************/
/* Neural Networks Code          */
/*                               */
/* Code by DrLabman              */
/*********************************/

#include "quakedef.h"
#include "NeuralNets.h"

int neural_nets;

neuralnetwork_t	*NN;

/*
======
Random

This function only returns a random number in the range (-1.0) - (1.0)
======
*/
float RandomWeight (void)
{
	return (rand ()&0x7ffe) / ((float)0x3fff) - 1.0f;
}

void NN_init(){
	int i;

	//check the command line to see if a number of neural networks was given
	i = COM_CheckParm ("-neuralnets");
	if (i){
		neural_nets = (int)(Q_atoi(com_argv[i+1]));
	} else {
		neural_nets = MAX_NEURAL_NETWORKS / 4;					//defualt to set 'max'
	}

	NN = (neuralnetwork_t *)Hunk_AllocName (neural_nets * sizeof(neuralnetwork_t), "neuralnets");

	for (i=0; i<neural_nets; i++){
		NN[i].free = true;
	}
}

//clean up on isle 4
//Get rid of any neural nets hanging.
void NN_deinit(){
	int i;

	for (i=0; i<neural_nets; i++){
		if (NN[i].free != true){
			NN_remove(i);
		}
	}	
}

//creates a neural network and returns an identifier to it
int NN_add(int inputs, int hidden, int outputs){
	int		ident, i;
	neuralnetwork_t	*temp;

	//do Neural Networks need to survive over map changes?

	//find the first free network on the list, we shouldnt be adding neural nets that much so O(n) is fine
	for (ident=0; ident<neural_nets; ident++){
		if (NN[ident].free){
			break;	//this one be free
		}
	}

	temp = &NN[ident];
	temp->free = false;	//now we be using it

	//store in quakes memory?
	//convert to one malloc?
	//allocate all arrays
	temp->inputs = (float *)malloc(sizeof(float)*(inputs+1));	//one extra for a bias weight
	temp->inputWeights = (float *)malloc(sizeof(float)*((inputs+1)*(inputs+1)));	//one extra for a bias weight
	temp->hiddens = (float *)malloc(sizeof(float)*(hidden+1));
	temp->hiddenWeights = (float *)malloc(sizeof(float)*((hidden+1)*(hidden+1)));
	temp->hiddenError = (float *)malloc(sizeof(float)*(hidden));
	temp->outputs = (float *)malloc(sizeof(float)*(outputs));
	temp->expectedOutputs = (float *)malloc(sizeof(float)*(outputs));

	//setup numbers
	temp->numOfInputs = inputs;
	temp->numOfHiddens = hidden;
	temp->numOfOutputs = outputs;

	temp->learningRate = 0.2f;	//default to smaller learning rate

	//setup bias
	temp->inputs[inputs] = 1.0f;
	temp->hiddens[hidden] = 1.0f;
    
	//randomise weights
	for (i=0; i<=inputs; i++){
		temp->inputWeights[i] = RandomWeight();
	}

	for (i=0; i<=hidden; i++){
		temp->hiddenWeights[i] = RandomWeight();
	}

	return ident;	//this is the ident to use this NN
}

//removes a neural network from memory
void NN_remove(int ident){
	neuralnetwork_t	*temp;

	temp = &NN[ident];

	//deallocate all arrays
	free(temp->inputs);
	free(temp->inputWeights);
	free(temp->hiddens);
	free(temp->hiddenWeights);
	free(temp->outputs);
	free(temp->expectedOutputs);

	//we are free!
	temp->free = true;
}

//set inputs to be what is in the array of floats
void NN_setInputs(int ident, int start, int n, float *inputs){
	int i;
	neuralnetwork_t	*temp;

	temp = &NN[ident];

	for(i=start; i<n; i++){
		temp->inputs[i] = inputs[i-start];
	}
}

//set input n to be value in input var
void NN_setInputN(int ident, int n, float input){
	if (n<NN[ident].numOfInputs){
		NN[ident].inputs[n] = input;
	} else {
		Con_Printf("Neural Network: Tried to access invalid input");
	}
}

//runs Neural Network using current inputs
void NN_run(int ident){
	int i,j,num_units,num_weights;
	neuralnetwork_t	*temp;
	float *from, *weights, *to;

	temp = &NN[ident];

	//add up input * weight for all input neurons
	//put values into outputs for single layer networks

	if (temp->numOfHiddens != 0){
		to = &temp->hiddens[0];
		from = &temp->inputs[0];
		weights = &temp->inputWeights[0];
		num_units = temp->numOfHiddens;
		num_weights = temp->numOfInputs;

		//for each hidden unit
		for (i=0; i<num_units; i++){
			to[i] = 0.0f;	//reset to 0
			//add up total weighting of inputs
			for (j=0; i<=num_weights; j++){
				to[i] += from[j] * weights[j];
			}
		}

		//setup for second layer
		to = &temp->outputs[0];
		from = &temp->hiddens[0];
		weights = &temp->hiddenWeights[0];
		num_units = temp->numOfOutputs;
		num_weights = temp->numOfHiddens;

	} else {
		//setup for single layer network
		to = &temp->outputs[0];
		from = &temp->inputs[0];
		weights = &temp->inputWeights[0];
		num_units = temp->numOfOutputs;
		num_weights = temp->numOfInputs;
	}

	//for each output unit
	for (i=0; i<num_units; i++){
		to[i] = 0.0f;	//reset to 0
		//add up total weighting of inputs
		for (j=0; i<=num_weights; j++){
			to[i] += from[j] * weights[j];
		}
	}	
}

//get output n from Neural Network
float NN_getOutputN(int ident, int n){
	if (n<NN[ident].numOfOutputs){
		return NN[ident].outputs[n];
	} else {
		return 0;
		Con_Printf("Neural Network: Tried to access invalid output");
	}
}

//get more than one output at a time
void NN_getOutputs(int ident, int start, int n, float *outputs){
	int i;
	neuralnetwork_t	*temp;

	temp = &NN[ident];

	for(i=start; i<n; i++){
		outputs[i-start] = temp->outputs[i];
	}
}

//set expected output n to be value passed in expected var for a Neural Network
void NN_setExpectedOutputs(int ident, int n, float expected){
	if (n<NN[ident].numOfOutputs){
		NN[ident].expectedOutputs[n] = expected;
	} else {
		Con_Printf("Neural Network: Tried to access invalid expected output");
	}
}

//run training selected training algo on a Neural Network(currently only supporting delta rule learning (backprop)
void NN_learn(int ident, int type){
	//ajust weights using detla rule
	neuralnetwork_t *temp;
	int i,j;
	float *desired, *actual, *input, *weight;//, *error;
	float learning;

	temp = &NN[ident];

	learning = temp->learningRate;

	if (temp->numOfHiddens != 0){
		//HIDDEN UNITS CURRENTLY NOT SUPPORTED
/*
		//reset hidden layer error sum
		for (i=0; i<temp->numOfHiddens; i++){
			temp->hiddenError[i] = 0;
		}

		//for each unit
		desired = &temp->expectedOutputs[0];
		actual = &temp->outputs[0];
		input = &temp->hiddens[0];
		weight = &temp->hiddenWeights[0];
		error = &temp->hiddenError[0];

		for (i=0; i<temp->numOfHiddens; i++){
			float delta = learning * (desired[i]-actual[i]);
			for (j=0; j<temp->numOfInputs; j++){
				weight[j] += delta * input[j];
				error[j] += delta * input[j];
			}
		}

		//for each unit
		desired = &temp->hiddelErrors[0];
		actual = &temp->outputs[0];
		input = &temp->inputs[0];
		weight = &temp->inputWeights[0];
		*/
	}
	
	//for each unit
	desired = &temp->expectedOutputs[0];
	actual = &temp->outputs[0];
	input = &temp->inputs[0];
	weight = &temp->inputWeights[0];

	for (i=0; i<temp->numOfOutputs; i++){
		float delta = learning * (desired[i]-actual[i]);
		for (j=0; j<temp->numOfInputs; j++){
			weight[j] += delta * input[j];
		}
	}
}

//sets the learning rate for a Neural Network, used by some training algos
void NN_setLearningRate(int ident, float rate){
	NN[ident].learningRate = rate;
}