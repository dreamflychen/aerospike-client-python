/*******************************************************************************
 * Copyright 2013-2014 Aerospike, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 ******************************************************************************/
#include <Python.h>
#include <stdbool.h>

#include <aerospike/aerospike_key.h>
#include <aerospike/as_key.h>
#include <aerospike/as_error.h>
#include <aerospike/as_record.h>

#include "client.h"
#include "conversions.h"
#include "key.h"
#include "policy.h"
#include "serializer.h"

uint32_t is_user_serializer_registered = 0;
uint32_t is_user_deserializer_registered = 0;

user_serializer_callback user_serializer_call_info, user_deserializer_call_info;

/**
 ******************************************************************************************************
 * Set a serializer in the aerospike database
 *
 * @param self                  Aerospike object
 * @param args                  The args is a tuple object containing an argument
 *                              list passed from Python to a C function
 * @param kwds                  Dictionary of keywords
 *
 * Returns  integer handle for the serializer being set.
 *******************************************************************************************************
 */
PyObject * AerospikeClient_Set_Serializer(AerospikeClient * self, PyObject * args, PyObject * kwds)
{
	// Python Function Arguments
	PyObject * py_func = NULL;

	// Python Function Keyword Arguments
	static char * kwlist[] = {"function", NULL};
	as_error err;
	// Initialize error
	as_error_init(&err);

	// Python Function Argument Parsing
	if ( PyArg_ParseTupleAndKeywords(args, kwds, "O:set_serializer", kwlist,
			&py_func) == false ) {
		return NULL;
	}

    if (!is_user_serializer_registered) {
        memset(&user_serializer_call_info, 0, sizeof(user_serializer_call_info));
    }

    if (user_serializer_call_info.callback == py_func) {
        return PyLong_FromLong(0);
    }
	if (!PyCallable_Check(py_func)) {
		as_error_update(&err, AEROSPIKE_ERR_PARAM, "Parameter must be a callable");
		goto CLEANUP;
	}

	if (user_serializer_call_info.callback != NULL) {
		Py_DECREF(user_serializer_call_info.callback);
	}
	is_user_serializer_registered = 1;
	user_serializer_call_info.callback = py_func;

CLEANUP:
	if ( err.code != AEROSPIKE_OK ) {
		PyObject * py_err = NULL;
		error_to_pyobject(&err, &py_err);
		PyErr_SetObject(PyExc_Exception, py_err);
        Py_DECREF(py_err);
		return NULL;
	}

	return PyLong_FromLong(0);
}

/**
 ******************************************************************************************************
 * Set a deserializer in the aerospike database
 *
 * @param self                  Aerospike object
 * @param args                  The args is a tuple object containing an argument
 *                              list passed from Python to a C function
 * @param kwds                  Dictionary of keywords
 *
 * Returns  integer handle for the deserializer being set
 *******************************************************************************************************
 */
PyObject * AerospikeClient_Set_Deserializer(AerospikeClient * self, PyObject * args, PyObject * kwds)
{
	// Python Function Arguments
	PyObject * py_func = NULL;

	// Python Function Keyword Arguments
	static char * kwlist[] = {"function", NULL};
	as_error err;
	as_error_init(&err);

	// Python Function Argument Parsing
	if ( PyArg_ParseTupleAndKeywords(args, kwds, "O:set_deserializer", kwlist,
			&py_func) == false ) {
		return NULL;
	}

    if (!is_user_deserializer_registered) {
        memset(&user_deserializer_call_info, 0, sizeof(user_deserializer_call_info));
    }

    if (user_deserializer_call_info.callback == py_func) {
        return PyLong_FromLong(0);
    }

	if (!PyCallable_Check(py_func)) {
		as_error_update(&err, AEROSPIKE_ERR_PARAM, "Parameter must be a callable");
		goto CLEANUP;
	}
	is_user_deserializer_registered = 1;
	if (user_deserializer_call_info.callback != NULL) {
		Py_DECREF(user_deserializer_call_info.callback);
	}
	user_deserializer_call_info.callback = py_func;

CLEANUP:
	if ( err.code != AEROSPIKE_OK ) {
		PyObject * py_err = NULL;
		error_to_pyobject(&err, &py_err);
		PyErr_SetObject(PyExc_Exception, py_err);
        Py_DECREF(py_err);
		return NULL;
	}

	return PyLong_FromLong(0);
}

/**
 ******************************************************************************************************
 * Initialize and set the bytes for serialization
 *
 * @param bytes                 The as_bytes object to be set.
 * @param bytes_string          The string which is to be loaded into the
 *                              as_bytes object.
 * @param bytes_string_len      The length of bytes_string
 * @param bytes_type            The type of bytes: AS_BYTES_BLOB or
 *                              AS_BYTES_PYTHON.
 * @param error_p               The error object
 ******************************************************************************************************
 */
void set_as_bytes(as_bytes **bytes,
                         uint8_t *bytes_string,
                         int32_t bytes_string_len,
                         int32_t bytes_type,
                         as_error *error_p)
{
    if((!bytes) || (!bytes_string)) {
        as_error_update(error_p, AEROSPIKE_ERR, "Unable to set as_bytes");
        goto CLEANUP;
    }

	as_bytes_init(*bytes, bytes_string_len);

    if (!as_bytes_set(*bytes, 0, bytes_string, bytes_string_len)) {
        as_error_update(error_p, AEROSPIKE_ERR, "Unable to set as_bytes");
    } else {
        as_bytes_set_type(*bytes, bytes_type);
    }

CLEANUP:

  if ( error_p->code != AEROSPIKE_OK ) {
        PyObject * py_err = NULL;
        error_to_pyobject(error_p, &py_err);
        PyErr_SetObject(PyExc_Exception, py_err);
        Py_DECREF(py_err);
	}
    return;
}

/*
 *******************************************************************************************************
 * If serialize_flag == true, executes the passed user_serializer_callback,
 * by creating as_bytes (bytes) from the passed Py_Object (value).
 * Else executes the passed user_deserializer_callback,
 * by passing the as_bytes (bytes) to the deserializer and getting back
 * the corresponding Py_Object (value).
 *
 * @param user_callback_info            The user_serializer_callback for the user
 *                                      callback to be executed.
 * @param bytes                         The as_bytes to be stored/retrieved.
 * @param value                         The value to be retrieved/stored.
 * @param serialize_flag                The flag which indicates
 *                                      serialize/deserialize.
 * @param error_p                       The as_error to be populated by the
 *                                      function with encountered error if any.
 *******************************************************************************************************
 */
void execute_user_callback(user_serializer_callback *user_callback_info,
                                  as_bytes **bytes,
                                  PyObject **value,
                                  bool serialize_flag,
                                  as_error *error_p)
{
	PyObject * py_return = NULL;
	PyObject * py_value = NULL;
	PyObject * py_arglist = PyTuple_New(1);
	int len;

    if (serialize_flag) {
		PyTuple_SetItem(py_arglist, 0 , *value);
    } else {
	    as_bytes * bytes_pointer = *bytes;
        char*       bytes_val_p = (char*)bytes_pointer->value;
	    py_value = PyString_FromStringAndSize(bytes_val_p, as_bytes_size(*bytes));
		PyTuple_SetItem(py_arglist, 0 , py_value);
    }

	py_return = PyEval_CallObject(user_callback_info->callback, py_arglist);
    if(!serialize_flag)
	Py_DECREF(py_arglist);
    if (py_return) {

        if (serialize_flag) {
			char * py_val = PyString_AsString(py_return);
			len = PyString_Size(py_return);
            set_as_bytes(bytes, (uint8_t *) py_val,
                         len, AS_BYTES_BLOB, error_p);
            Py_DECREF(py_return);
        } else {
			*value = py_return;
		}
    } else {
        if (serialize_flag) {
            as_error_update(error_p, AEROSPIKE_ERR,
                    "Unable to call user's registered serializer callback");
			goto CLEANUP;
        } else {
            as_error_update(error_p, AEROSPIKE_ERR,
                    "Unable to call user's registered deserializer callback");
			goto CLEANUP;
        }
    }

CLEANUP:
  if ( error_p->code != AEROSPIKE_OK ) {
        PyObject * py_err = NULL;
        error_to_pyobject(error_p, &py_err);
        PyErr_SetObject(PyExc_Exception, py_err);
        Py_DECREF(py_err);
    }
}

/*
 *******************************************************************************************************
 * Checks serializer_policy.
 * Serializes Py_Object (value) into as_bytes using serialization logic
 * based on serializer_policy.
 *
 * @param serializer_policy         The serializer_policy to be used to handle
 *                                  the serialization.
 * @param bytes                     The as_bytes to be set.
 * @param value                     The value to be serialized.
 * @param error_p                   The as_error to be populated by the function
 *                                  with encountered error if any.
 *******************************************************************************************************
 */
extern PyObject * serialize_based_on_serializer_policy(int32_t serializer_policy,
		as_bytes **bytes,
		PyObject *value,
		as_error *error_p)
{
    switch(serializer_policy) {
        case SERIALIZER_NONE:
            as_error_update(error_p, AEROSPIKE_ERR_PARAM,
                    "Cannot serialize: SERIALIZER_NONE selected");
            goto CLEANUP;
        case SERIALIZER_PYTHON:
            {
				/* get the sys.modules dictionary */
				PyObject* sysmodules = PyImport_GetModuleDict();
				PyObject* cpickle_module = NULL;
				if(PyMapping_HasKeyString(sysmodules, "cPickle")) {
    				cpickle_module = PyMapping_GetItemString(sysmodules, "cPickle");
				} else {
    				cpickle_module = PyImport_ImportModule("cPickle");
				}

    			PyObject* initresult;

    			if(!cpickle_module) {
      			/* insert error handling here! and exit this function */
					as_error_update(error_p, AEROSPIKE_ERR_CLIENT, "Unable to load cpickle module");
					goto CLEANUP;
    			} else {
					PyObject * py_funcname = PyString_FromString("dumps");
					initresult = PyObject_CallMethodObjArgs(cpickle_module,
py_funcname, value, NULL);
					Py_DECREF(py_funcname);
    				if(!initresult) {
      				/* more error handling &c */
						as_error_update(error_p, AEROSPIKE_ERR_CLIENT, "Unable to call dumps function");
						goto CLEANUP;
    				} else {
						char *return_value = PyString_AsString(initresult);
                        int len = strlen(return_value);
						set_as_bytes(bytes, (uint8_t *) return_value,
								len, AS_BYTES_PYTHON, error_p);
    					Py_DECREF(initresult);
					}

				Py_DECREF(cpickle_module);
			}
		}
            break;
        case SERIALIZER_JSON:
			/*
            *   TODO:
            *     Handle JSON serialization after support for AS_BYTES_JSON
            *     is added in aerospike-client-c
            */
            as_error_update(error_p, AEROSPIKE_ERR,
                         "Unable to serialize using standard json serializer");
            goto CLEANUP;

        case SERIALIZER_USER:
            if (is_user_serializer_registered) {
				execute_user_callback(&user_serializer_call_info, bytes, &value, true, error_p);
                if (AEROSPIKE_OK != (error_p->code)) {
                    goto CLEANUP;
                }
            } else {
                as_error_update(error_p, AEROSPIKE_ERR,
                        "No serializer callback registered");
                goto CLEANUP;
            }
            break;
        default:
            as_error_update(error_p, AEROSPIKE_ERR,
                    "Unsupported serializer");
            goto CLEANUP;
    }

CLEANUP:

  if ( error_p->code != AEROSPIKE_OK ) {
        PyObject * py_err = NULL;
        error_to_pyobject(error_p, &py_err);
        PyErr_SetObject(PyExc_Exception, py_err);
        Py_DECREF(py_err);
		return NULL;
    }
  
  return PyLong_FromLong(0);
}

/*
 *******************************************************************************************************
 * Checks as_bytes->type.
 * Deserializes as_bytes into Py_Object (retval) using deserialization logic
 * based on as_bytes->type.
 *
 * @param bytes                 The as_bytes to be deserialized.
 * @param retval                The return zval to be populated with the
 *                              deserialized value of the input as_bytes.
 * @param error_p               The as_error to be populated by the function
 *                              with encountered error if any.
 *******************************************************************************************************
 */
extern PyObject * deserialize_based_on_as_bytes_type(as_bytes  *bytes,
		PyObject  **retval,
		as_error  *error_p)
{
    switch(as_bytes_get_type(bytes)) {
        case AS_BYTES_PYTHON: {
			PyObject* sysmodules = PyImport_GetModuleDict();
			PyObject* cpickle_module = NULL;
			if(PyMapping_HasKeyString(sysmodules, "cPickle")) {
    			cpickle_module = PyMapping_GetItemString(sysmodules, "cPickle");
			} else {
    			cpickle_module = PyImport_ImportModule("cPickle");
			}

    		PyObject* initresult;
    		if(!cpickle_module) {
      			/* insert error handling here! and exit this function */
				as_error_update(error_p, AEROSPIKE_ERR_CLIENT, "Unable to load cpickle module");
				goto CLEANUP;
    		} else {
    			char*       bytes_val_p = (char*)bytes->value;
				PyObject *py_value = PyString_FromStringAndSize(bytes_val_p, as_bytes_size(bytes));
				PyObject *py_funcname = PyString_FromString("loads");
				initresult = PyObject_CallMethodObjArgs(cpickle_module,
py_funcname, py_value, NULL);
				Py_DECREF(py_funcname);
                Py_DECREF(py_value);
    			if(!initresult) {
      			/* more error handling &c */
					as_error_update(error_p, AEROSPIKE_ERR_CLIENT, "Unable to call dumps function");
					goto CLEANUP;
    			} else {
					*retval = initresult;
				}

			Py_DECREF(cpickle_module);
            }
			}
            break;
        case AS_BYTES_BLOB: {
                if (is_user_deserializer_registered) {
					execute_user_callback(&user_deserializer_call_info, &bytes, retval, false, error_p);
                    if(AEROSPIKE_OK != (error_p->code)) {
                        goto CLEANUP;
                    }
                } else {
					uint32_t bval_size = as_bytes_size(bytes);
					PyObject *py_val = PyByteArray_FromStringAndSize((char *) as_bytes_get(bytes), bval_size);
					if (!py_val) {
						as_error_update(error_p, AEROSPIKE_ERR_CLIENT, "Unable to deserialize bytes");
						goto CLEANUP;
					}
					*retval = py_val;
                }
            }
            break;
        default:

            as_error_update(error_p, AEROSPIKE_ERR,
                    "Unable to deserialize bytes");
            goto CLEANUP;
    }

CLEANUP:

  if ( error_p->code != AEROSPIKE_OK ) {
        PyObject * py_err = NULL;
        error_to_pyobject(error_p, &py_err);
        PyErr_SetObject(PyExc_Exception, py_err);
        Py_DECREF(py_err);
		return NULL;
    }

  return PyLong_FromLong(0);
}
