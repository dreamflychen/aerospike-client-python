# -*- coding: utf-8 -*-

import pytest
import sys
from test_base_class import TestBaseClass

try:
    import aerospike
except:
    print "Please install aerospike python client."
    sys.exit(1)

class TestSelectMany(object):
    def setup_class(cls):
        """
        Setup method.
        """
        hostlist, user, password = TestBaseClass.get_hosts()
        config = {
                'hosts': hostlist
                }
        if user == None and password == None:
            TestSelectMany.client = aerospike.client(config).connect()
        else:
            TestSelectMany.client = aerospike.client(config).connect(user, password)

    def teardown_class(cls):
        TestSelectMany.client.close()

    def setup_method(self, method):
        self.keys = []

        for i in xrange(5):
            key = ('test', 'demo', i)
            rec = {
                    'title': 'Mr.',
                    'name' : 'name%s' % (str(i)),
                    'age'  : i,
                    'addr' : 'Minisota',
                    'country': 'USA'
                    }
            TestSelectMany.client.put(key, rec)
            self.keys.append(key)


    def teardown_method(self, method):

        """
        Teardown method.
        """
        for i in xrange(5):
            key = ('test', 'demo', i)
            TestSelectMany.client.remove(key)

    def test_select_many_without_any_parameter(self):

        with pytest.raises(TypeError) as typeError:
            TestSelectMany.client.select_many()

        assert "Required argument 'keys' (pos 1) not found" in typeError.value

    def test_select_many_without_policy(self):

        filter_bins = [ 'title', 'name' ]
        records = TestSelectMany.client.select_many( self.keys, filter_bins )

        assert type(records) == dict
        assert len(records.keys()) == 5
        for k in records.keys():
            bins =  records[k][2].keys()
            assert set(bins).intersection(set(filter_bins)) == set(bins)

    def test_select_many_with_proper_parameters(self):

        filter_bins = [ 'title', 'name' ]
        records = TestSelectMany.client.select_many( self.keys, filter_bins, { 'timeout': 3 } )

        assert type(records) == dict
        assert len(records.keys()) == 5
        assert records.keys() == [0, 1, 2, 3, 4]
        for k in records.keys():
            bins =  records[k][2].keys()
            assert set(bins).intersection(set(filter_bins)) == set(bins)

    def test_select_many_with_none_policy(self):

        filter_bins = [ 'name' ]
        records = TestSelectMany.client.select_many( self.keys, filter_bins, None )

        assert type(records) == dict
        assert len(records.keys()) == 5
        assert records.keys() == [0, 1, 2, 3, 4]
        for k in records.keys():
            bins =  records[k][2].keys()
            assert set(bins).intersection(set(filter_bins)) == set(bins)

    def test_select_many_with_none_keys(self):

        with pytest.raises(Exception) as exception:
            TestSelectMany.client.select_many( None, [], {} )

        assert exception.value[0] == -1
        assert exception.value[1] == "Keys should be specified as a list or tuple."

    def test_select_many_with_non_existent_keys(self):

        self.keys.append( ('test', 'demo', 'non-existent') )

        filter_bins = [ 'title', 'name', 'addr' ]
        records = TestSelectMany.client.select_many( self.keys, filter_bins, {'timeout': 1000} )

        assert type(records) == dict
        assert len(records.keys()) == 6
        assert records.keys() == [0, 1, 2, 3, 4, 'non-existent']
        assert records['non-existent'] == None
        for k in records.keys():
            if records[k] == None: continue
            bins =  records[k][2].keys()
            assert set(bins).intersection(set(filter_bins)) == set(bins)

    def test_select_many_with_all_non_existent_keys(self):

        keys = [( 'test', 'demo', 'key' )]

        filter_bins = [ 'title', 'name', 'country' ]
        records = TestSelectMany.client.select_many( keys, filter_bins )

        assert len(records.keys()) == 1
        assert records == {'key': None}
        for k in records.keys():
            if records[k] == None: continue
            bins =  records[k][2].keys()
            assert set(bins).intersection(set(filter_bins)) == set(bins)

    def test_select_many_with_invalid_key(self):

        with pytest.raises(Exception) as exception:
            records = TestSelectMany.client.select_many( "key", [] )

        assert exception.value[0] == -1
        assert exception.value[1] == "Keys should be specified as a list or tuple."

    def test_select_many_with_invalid_timeout(self):

        policies = { 'timeout' : 0.2 }
        with pytest.raises(Exception) as exception:
            records = TestSelectMany.client.select_many(self.keys, [], policies)

        assert exception.value[0] == -2
        assert exception.value[1] == "timeout is invalid"

    def test_select_many_with_initkey_as_digest(self):

        keys = []
        key = ("test", "demo", None, bytearray("asd;as[d'as;djk;uyfl"))
        rec = {
            'name' : 'name1',
            'age'  : 1
        }
        TestSelectMany.client.put(key, rec)
        keys.append(key)

        key = ("test", "demo", None, bytearray("ase;as[d'as;djk;uyfl"))
        rec = {
            'name' : 'name2',
            'age'  : 2
        }
        TestSelectMany.client.put(key, rec)
        keys.append(key)

        records = TestSelectMany.client.select_many( keys, [ u'name' ] )

        for key in keys:
            TestSelectMany.client.remove( key )

        assert type(records) == dict
        assert len(records.keys()) == 2

    def test_select_many_with_non_existent_keys_in_middle(self):

        self.keys.append( ('test', 'demo', 'some_key') )

        for i in xrange(15,20):
            key = ('test', 'demo', i)
            rec = {
                    'name' : 'name%s' % (str(i)),
                    'age'  : i,
                    'position' : 'Sr. Engineer'
                    }
            TestSelectMany.client.put(key, rec)
            self.keys.append(key)

        filter_bins = [ 'title', 'name', 'position' ]
        records = TestSelectMany.client.select_many( self.keys, filter_bins )

        for i in xrange(15,20):
            key = ('test', 'demo', i)
            TestSelectMany.client.remove(key)

        assert type(records) == dict
        assert len(records.keys()) == 11
        assert records.keys() == [0, 1, 2, 3, 4, 'some_key', 15, 16, 17, 18, 19]
        assert records['some_key'] == None
        for k in records.keys():
            if records[k] == None: continue
            bins =  records[k][2].keys()
            assert set(bins).intersection(set(filter_bins)) == set(bins)

    def test_select_many_with_unicode_bins(self):

        filter_bins = [ u'title', u'name', 'country', u'addr' ]
        records = TestSelectMany.client.select_many( self.keys, filter_bins )

        assert type(records) == dict
        assert len(records.keys()) == 5
        for k in records.keys():
            bins =  records[k][2].keys()
            assert set(bins).intersection(set(filter_bins)) == set(bins)

    def test_select_many_with_empty_bins_list(self):

        records = TestSelectMany.client.select_many( self.keys, [] )

        assert type(records) == dict
        assert len(records.keys()) == 5

    def test_select_many_with_proper_parameters_without_connection(self):

        config = {
                'hosts': [('127.0.0.1', 3000)]
                }
        client1 = aerospike.client(config)

        filter_bins = [ 'title', 'name' ]

        with pytest.raises(Exception) as exception:
            records = client1.select_many( self.keys, filter_bins, { 'timeout': 3 } )

        assert exception.value[0] == 11L
        assert exception.value[1] == 'No connection to aerospike cluster'
