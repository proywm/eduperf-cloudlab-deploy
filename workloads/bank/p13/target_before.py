from typing import Any, List, Tuple


class Datapoint:
    def __init__(self, **kwargs):
        self.timestamp = kwargs.get("timestamp")
        self.value = kwargs.get("value")
        self.average = kwargs.get("average")
        self.max = kwargs.get("max")
        self.min = kwargs.get("min")
        self.count = kwargs.get("count")
        self.sum = kwargs.get("sum")
        self.interpolation = kwargs.get("interpolation")
        self.step_interpolation = kwargs.get("step_interpolation")
        self.continuous_variance = kwargs.get("continuous_variance")
        self.discrete_variance = kwargs.get("discrete_variance")
        self.total_variation = kwargs.get("total_variation")


class Datapoints:
    def __init__(self, **kwargs):
        self.id = kwargs.get("id")
        self.external_id = kwargs.get("external_id")
        self.is_string = kwargs.get("is_string")
        self.is_step = kwargs.get("is_step")
        self.unit = kwargs.get("unit")
        self.timestamp = kwargs.get("timestamp") or []
        self.value = kwargs.get("value")
        self.average = kwargs.get("average")
        self.max = kwargs.get("max")
        self.min = kwargs.get("min")
        self.count = kwargs.get("count")
        self.sum = kwargs.get("sum")
        self.interpolation = kwargs.get("interpolation")
        self.step_interpolation = kwargs.get("step_interpolation")
        self.continuous_variance = kwargs.get("continuous_variance")
        self.discrete_variance = kwargs.get("discrete_variance")
        self.total_variation = kwargs.get("total_variation")
        self.__datapoint_objects = None

    def __len__(self) -> int:
        return len(self.timestamp)

    def _get_non_empty_data_fields(self, get_empty_lists=False) -> List[Tuple[str, Any]]:
        non_empty_data_fields = []
        for attr, value in self.__dict__.copy().items():
            if attr not in ["id", "external_id", "is_string", "is_step", "unit"] and attr[0] != "_":
                if value is not None or attr == "timestamp":
                    if len(value) > 0 or get_empty_lists or attr == "timestamp":
                        non_empty_data_fields.append((attr, value))
        return non_empty_data_fields

    def get_datapoint_objects(self) -> List[Datapoint]:
        if self.__datapoint_objects is None:
            self.__datapoint_objects = []
            for i in range(len(self)):
                dp_args = {}
                for attr, value in self._get_non_empty_data_fields():
                    dp_args[attr] = value[i]
                self.__datapoint_objects.append(Datapoint(**dp_args))
        return self.__datapoint_objects
