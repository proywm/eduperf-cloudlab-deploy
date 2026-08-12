class InvalidAggregationMethod(Exception):
  pass


def aggregate(aggregationMethod, knownValues, neighborValues=None):
  if aggregationMethod == 'average':
    return float(sum(knownValues)) / float(len(knownValues))
  elif aggregationMethod == 'sum':
    return float(sum(knownValues))
  elif aggregationMethod == 'last':
    return knownValues[-1]
  elif aggregationMethod == 'max':
    return max(knownValues)
  elif aggregationMethod == 'min':
    return min(knownValues)
  elif aggregationMethod == 'avg_zero':
    if not neighborValues:
        raise InvalidAggregationMethod("Using avg_zero without neighborValues")
    values = [x or 0 for x in neighborValues]
    return float(sum(values)) / float(len(values))
  else:
    raise InvalidAggregationMethod("Unrecognized aggregation method %s" %
            aggregationMethod)
