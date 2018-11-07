import { TestBed } from '@angular/core/testing';

import { configureTestBed, i18nProviders } from '../../../testing/unit-test-helper';
import { MdsSummaryPipe } from './mds-summary.pipe';

describe('MdsSummaryPipe', () => {
  let pipe: MdsSummaryPipe;

  configureTestBed({
    providers: [MdsSummaryPipe, i18nProviders]
  });

  beforeEach(() => {
    pipe = TestBed.get(MdsSummaryPipe);
  });

  it('create an instance', () => {
    expect(pipe).toBeTruthy();
  });

  it('transforms with 0 active and 2 standy', () => {
    const value = {
      standbys: [0],
      filesystems: [{ mdsmap: { info: [{ state: 'up:standby-replay' }] } }]
    };
    expect(pipe.transform(value)).toBe('0 active, 2 standby');
  });

  it('transforms with 1 active and 1 standy', () => {
    const value = {
      standbys: [0],
      filesystems: [{ mdsmap: { info: [{ state: 'up:active' }] } }]
    };
    expect(pipe.transform(value)).toBe('1 active, 1 standby');
  });

  it('transforms with 0 filesystems', () => {
    const value = {
      standbys: [0],
      filesystems: []
    };
    expect(pipe.transform(value)).toBe('no filesystems');
  });

  it('transforms without filesystem', () => {
    const value = { standbys: [0] };
    expect(pipe.transform(value)).toBe('1, no filesystems');
  });

  it('transforms without value', () => {
    expect(pipe.transform(undefined)).toBe('');
  });
});
