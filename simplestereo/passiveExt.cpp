/* Custom implementation of
 * Adaptive Support Weight from "Locally adaptive support-weight approach
 * for visual correspondence search", K. Yoon, I. Kweon, 2005.
 * */
#define PY_SSIZE_T_CLEAN
#include <Python.h>
#include <numpy/arrayobject.h>
 
#include <iostream>
#include <math.h>
#include <thread>
#include "headers/SafeQueue.hpp"


void workerASW(SafeQueue<int> &jobs, npy_ubyte *data1, npy_ubyte *data2, npy_float *dataLab1, npy_float *dataLab2,
            npy_int *disparityMap, float *proximityWeights, int gammaC,
            int width, int height, int winSize, int padding,
            int minDisparity, int maxDisparity)
{
    int dBest;
    float cost, costBest, tot;
    int ii,jj,kk;
    int i,j,y,x,d;
    float w1[winSize][winSize], w2[winSize][winSize]; // Weights
    
    
    while(!jobs.empty()) 
    {
        
    jobs.pop(y); // Get element, put it in y and remove from queue
    
    for(x=0; x < width; ++x) {      // For each column on left image
            
            // Pre-compute weights for left window
            for(i = 0; i < winSize; ++i) {
                ii = std::min(std::max(y-padding+i,0),height-1); // Ensure to be within image, replicate border if not
                for(j = 0; j < winSize; ++j) {
                    jj = std::min(std::max(x-padding+j,0),width-1);
                    
                    w1[i][j] = proximityWeights[i*winSize + j] * 
                               exp(-sqrt( fabs(dataLab1[3*(ii*width + jj)] - dataLab1[3*(y*width+x)]) +
                                          fabs(dataLab1[3*(ii*width + jj)+1] - dataLab1[3*(y*width+x)+1]) + 
                                          fabs(dataLab1[3*(ii*width + jj)+2] - dataLab1[3*(y*width+x)+2]) )/gammaC);
                }
            }
            
            dBest = 0;
            costBest = INFINITY; // Initialize cost to an high value
            for(d = x-minDisparity; d >= std::max(0,x-maxDisparity); --d) {  // For each allowed disparity (reverse order)
                
                cost = 0;   // Cost of current match
                tot = 0;    // Sum of weights
                for(i = 0; i < winSize; ++i) {
                    ii = std::min(std::max(y-padding+i,0),height-1);
                    for(j = 0; j < winSize; ++j) {
                        jj = std::min(std::max(d-padding+j,0),width-1);
                        kk = std::min(std::max(x-padding+j,0),width-1);
                        
                        // Build weight
                        w2[i][j] = proximityWeights[i*winSize + j] * 
                                   exp(-sqrt( fabs(dataLab2[3*(ii*width + jj)] - dataLab2[3*(y*width+d)]) +
                                              fabs(dataLab2[3*(ii*width + jj)+1] - dataLab2[3*(y*width+d)+1]) + 
                                              fabs(dataLab2[3*(ii*width + jj)+2] - dataLab2[3*(y*width+d)+2]) )/gammaC);
                        
                        // Update cost
                        cost += w1[i][j]*w2[i][j]*( abs(data1[3*(ii*width + kk)] - data2[3*(ii*width + jj)]) + 
                                                    abs(data1[3*(ii*width + kk)+1] - data2[3*(ii*width + jj)+1]) + 
                                                    abs(data1[3*(ii*width + kk)+2] - data2[3*(ii*width + jj)+2]) );
                        // And denominator
                        tot += w1[i][j]*w2[i][j];
                        
                    }
                }
                
                // Weighted average
                cost = cost / tot;
                
                if(cost < costBest) {
                    costBest = cost;
                    dBest = d;
                    }
                
            }
            
            // Update disparity
            disparityMap[y*width + x] = x-dBest;
            
        }
    
    
    } // End of while
    
}


void workerASWconsistent(SafeQueue<int> &jobs, npy_ubyte *data1, npy_ubyte *data2, npy_float *dataLab1, npy_float *dataLab2,
            npy_int *disparityMap, float *proximityWeights, int gammaC,
            int width, int height, int winSize, int padding,
            int minDisparity, int maxDisparity)
{
    int dBest;
    float cost, costBest, tot;
    int ii,jj,kk;
    int i,j,y,x,d;
    float w1[winSize][winSize], w2[winSize][winSize]; // Weights
    int left,right,k;
    
    while(!jobs.empty()) 
    {
        
    jobs.pop(y); // Get element, put it in y and remove from queue
    
    // TEMP
    //printf("Working on %d\n", y);
    
    for(x=0; x < width; ++x) {      // For each column on left image
            
            // Pre-compute weights for left window
            for(i = 0; i < winSize; ++i) {
                ii = std::min(std::max(y-padding+i,0),height-1); // Ensure to be within image, replicate border if not
                for(j = 0; j < winSize; ++j) {
                    jj = std::min(std::max(x-padding+j,0),width-1);
                    
                    w1[i][j] = proximityWeights[i*winSize + j] * 
                               exp(-sqrt( fabs(dataLab1[3*(ii*width + jj)] - dataLab1[3*(y*width+x)]) +
                                          fabs(dataLab1[3*(ii*width + jj)+1] - dataLab1[3*(y*width+x)+1]) + 
                                          fabs(dataLab1[3*(ii*width + jj)+2] - dataLab1[3*(y*width+x)+2]) )/gammaC);
                }
            }
            
            dBest = 0;
            costBest = INFINITY; // Initialize cost to an high value
            for(d = x-minDisparity; d >= std::max(0,x-maxDisparity); --d) {  // For each allowed disparity ON RIGHT (reverse order)
                
                cost = 0;   // Cost of current match
                tot = 0;    // Sum of weights
                for(i = 0; i < winSize; ++i) {
                    ii = std::min(std::max(y-padding+i,0),height-1);
                    for(j = 0; j < winSize; ++j) {
                        jj = std::min(std::max(d-padding+j,0),width-1);
                        kk = std::min(std::max(x-padding+j,0),width-1);
                        
                        // Build weight
                        w2[i][j] = proximityWeights[i*winSize + j] * 
                                   exp(-sqrt( fabs(dataLab2[3*(ii*width + jj)] - dataLab2[3*(y*width+d)]) +
                                              fabs(dataLab2[3*(ii*width + jj)+1] - dataLab2[3*(y*width+d)+1]) + 
                                              fabs(dataLab2[3*(ii*width + jj)+2] - dataLab2[3*(y*width+d)+2]) )/gammaC);
                        
                        // Update cost
                        cost += w1[i][j]*w2[i][j]*( abs(data1[3*(ii*width + kk)] - data2[3*(ii*width + jj)]) + 
                                                    abs(data1[3*(ii*width + kk)+1] - data2[3*(ii*width + jj)+1]) + 
                                                    abs(data1[3*(ii*width + kk)+2] - data2[3*(ii*width + jj)+2]) );
                        // And denominator
                        tot += w1[i][j]*w2[i][j];
                        
                    }
                }
                
                // Weighted average
                cost = cost / tot;
                
                if(cost < costBest) {
                    costBest = cost;
                    dBest = d;
                    }
                
            }
            
            // Update disparity
            disparityMap[y*width + x] = x-dBest;
            
        }
        
    // Consistency check *****************
    for(x=0; x < width; ++x) {      // For each column on RIGHT image
            
        // Pre-compute weights for RIGHT window
        for(i = 0; i < winSize; ++i) {
            ii = std::min(std::max(y-padding+i,0),height-1); // Ensure to be within image, replicate border if not
            for(j = 0; j < winSize; ++j) {
                jj = std::min(std::max(x-padding+j,0),width-1);
                
                w2[i][j] = proximityWeights[i*winSize + j] * 
                           exp(-sqrt( fabs(dataLab2[3*(ii*width + jj)] - dataLab2[3*(y*width+x)]) +
                                      fabs(dataLab2[3*(ii*width + jj)+1] - dataLab2[3*(y*width+x)+1]) + 
                                      fabs(dataLab2[3*(ii*width + jj)+2] - dataLab2[3*(y*width+x)+2]) )/gammaC);
            }
        }
        
        dBest = 0;
        costBest = INFINITY; // Initialize cost to an high value
        for(d = x+minDisparity; d <= std::min(width-1,x+maxDisparity); ++d) {  // For each allowed disparity ON LEFT
            
            cost = 0;   // Cost of current match
            tot = 0;    // Sum of weights
            for(i = 0; i < winSize; ++i) {
                ii = std::min(std::max(y-padding+i,0),height-1);
                for(j = 0; j < winSize; ++j) {
                    jj = std::min(std::max(d-padding+j,0),width-1);
                    kk = std::min(std::max(x-padding+j,0),width-1);
                    
                    // Build weight
                    w1[i][j] = proximityWeights[i*winSize + j] * 
                               exp(-sqrt( fabs(dataLab1[3*(ii*width + jj)] - dataLab1[3*(y*width+d)]) +
                                          fabs(dataLab1[3*(ii*width + jj)+1] - dataLab1[3*(y*width+d)+1]) + 
                                          fabs(dataLab1[3*(ii*width + jj)+2] - dataLab1[3*(y*width+d)+2]) )/gammaC);
                    
                    // Update cost
                    cost += w1[i][j]*w2[i][j]*( abs(data2[3*(ii*width + kk)] - data1[3*(ii*width + jj)]) + 
                                                abs(data2[3*(ii*width + kk)+1] - data1[3*(ii*width + jj)+1]) + 
                                                abs(data2[3*(ii*width + kk)+2] - data1[3*(ii*width + jj)+2]) );
                    // And denominator
                    tot += w1[i][j]*w2[i][j];
                    
                }
            }
            
            // Weighted average
            cost = cost / tot;
            
            if(cost < costBest) {
                costBest = cost;
                dBest = d;
                }
            
        }
        
        // Update disparity map (dBest-x is the disparity, dBest is the best x coordinate on img1)
        if(disparityMap[y*width + dBest] != dBest-x) // Check if equal to first calculation
            disparityMap[y*width + dBest] = -1;       // Invalidated pixel!
        }
        
    
    // Left-Right consistency check
    // Disparity value == -1 means invalidated (occluded) pixel
    for(j=0; j < width; ++j) {
        if(disparityMap[y*width + j] == -1){
            // Find limits
            left = j-1;
            right = j+1;
            while(left>=0 and disparityMap[y*width + left] == -1){
                --left;
                }
            while(right<width and disparityMap[y*width + right] == -1){
                ++right;
                }
            // Left and right contain the first non occluded pixel in that direction
            // Ensure that we are within image limits
            // and assing valid value to occluded pixels
            if(left < 0){
                for(k=0;k<right;++k)
                    disparityMap[y*width + k] = disparityMap[y*width + right];
                }
            else if(right > width-1){
                for(k=left+1;k<width;++k)
                    disparityMap[y*width + k] = disparityMap[y*width + left];
                }
            else{
                for(k=left+1;k<right;++k)
                    disparityMap[y*width + k] = std::min(disparityMap[y*width + left],disparityMap[y*width + right]);
                }
            }
        }
    
    
    }  // End of while
}


PyObject *computeASW(PyObject *self, PyObject *args)
{
    PyArrayObject *img1, *img2, *img1Lab, *img2Lab;
    int winSize, maxDisparity, minDisparity, gammaC, gammaP;
    int consistent = 0; // Optional value
    
    // Parse input. See https://docs.python.org/3/c-api/arg.html
    if (!PyArg_ParseTuple(args, "OOOOiiiii|p", &img1, &img2, &img1Lab, &img2Lab, 
                          &winSize, &maxDisparity, &minDisparity, &gammaC, &gammaP,
                          &consistent)){
        PyErr_SetString(PyExc_ValueError, "Invalid input format!");
        return NULL;
        }
    
    // Check input format
    if (!(PyArray_TYPE(img1) == NPY_UBYTE and PyArray_TYPE(img1) == NPY_UBYTE)){
        // Raising an exception in C is done by setting the exception object or string and then returning NULL from the function.
        // See https://docs.python.org/3/c-api/exceptions.html
        PyErr_SetString(PyExc_TypeError, "Wrong type input!");
        return NULL;
        }
    if (PyArray_NDIM(img1)!=3 or PyArray_NDIM(img1)!=PyArray_NDIM(img2) or 
        PyArray_DIM(img1,2)!=3  or PyArray_DIM(img2,2)!=3){
        PyErr_SetString(PyExc_ValueError, "Wrong image dimensions!");
        return NULL;
        }
    if (!(winSize>0 and winSize%2==1)) {
        PyErr_SetString(PyExc_ValueError, "winSize must be a positive odd number!");
        return NULL;
        }
    
    
    //Retrieve input
    int height = PyArray_DIM(img1,0);
    int width = PyArray_DIM(img1,1);
    
    // See https://numpy.org/devdocs/reference/c-api/dtype.html
    npy_ubyte *data1 = (npy_ubyte *)PyArray_DATA(img1);       // Pointer to first element (casted to right type!)
    npy_ubyte *data2 = (npy_ubyte *)PyArray_DATA(img2);       // These are 1D arrays, (f**k)!
    npy_float *dataLab1 = (npy_float *)PyArray_DATA(img1Lab); // No elegant way to see them as data[height][width][color]?
    npy_float *dataLab2 = (npy_float *)PyArray_DATA(img2Lab);
    
    // Initialize disparity map
    npy_intp disparityMapDims[2] = {height, width};
    PyArrayObject *disparityMapObj = (PyArrayObject*)PyArray_EMPTY(2, disparityMapDims, NPY_INT,0);
    npy_int *disparityMap = (npy_int *)PyArray_DATA(disparityMapObj); // Pointer to first element
    
    // Working variables
    int padding = winSize / 2;
    int i,j;
    SafeQueue<int> jobs; // Jobs queue
    int num_threads = std::thread::hardware_concurrency();
    
    std::thread workersArr[num_threads];
    
    // Build proximity weights matrix
    float proximityWeights[winSize][winSize];
    
    for(i = 0; i < winSize; ++i) {
        for(j = 0; j < winSize; ++j) {
            proximityWeights[i][j] = exp(-sqrt( pow(i-padding,2) + pow(j-padding,2))/gammaP);
        }
    }
    
    
    // Put each image row in queue
    for(i=0; i < height; ++i) {   
        jobs.push(i);
    }
    
    
    if(!consistent) {
    // Start workers
    for(i = 0; i < num_threads; ++i) {
        workersArr[i] = std::thread( workerASW, std::ref(jobs), data1, data2, dataLab1, dataLab2,
                                          disparityMap, &proximityWeights[0][0], gammaC,             // Don't know why usign "proximityWeights" only does not work
                                          width, height, winSize, padding, minDisparity, maxDisparity);
        }
    } else { // If consistent mode is chosen
        
        // Start consistent workers
        for(i = 0; i < num_threads; ++i) {
            workersArr[i] = std::thread( workerASWconsistent, std::ref(jobs), data1, data2, dataLab1, dataLab2,
                                          disparityMap, &proximityWeights[0][0], gammaC,             // Don't know why usign "proximityWeights" only does not work
                                          width, height, winSize, padding, minDisparity, maxDisparity);
        }
    }
    
    // Join threads
    for(i = 0; i < num_threads; ++i) {
        workersArr[i].join();
    }    
    
    
    
    
    
    // Cast to PyObject and return (apparently you cannot return a PyArrayObject)
    return (PyObject*)disparityMapObj;  
    
}






/*MODULE INITIALIZATION*/
static struct PyMethodDef module_methods[] = {
    {"computeASW", computeASW, METH_VARARGS, NULL},             //nameout,functionname, METH_VARARGS, NULL
    { NULL,NULL,0, NULL}
};



static struct PyModuleDef moduledef = {
        PyModuleDef_HEAD_INIT,
        "passiveExt",
        NULL,
        -1,
        module_methods,
        NULL,
        NULL,
        NULL,
        NULL
};

PyMODINIT_FUNC PyInit_passiveExt(void)
{
    PyObject *m;
    
    import_array(); //This function must be called to use Numpy C-API
    
    m = PyModule_Create(&moduledef);
    if (m == NULL) {
        return NULL;
    }
    
    

    return m;
}

