import { ComponentFixture, TestBed, waitForAsync } from '@angular/core/testing';
import { ɵMockMatchMediaProvider } from '@angular/flex-layout';

import { ComponentsModule } from '~/app/core/components/components.module';
import { WorkbenchLayoutComponent } from '~/app/core/components/layouts/workbench-layout/workbench-layout.component';
import { TestingModule } from '~/app/testing.module';

describe('WorkbenchLayoutComponent', () => {
  let component: WorkbenchLayoutComponent;
  let fixture: ComponentFixture<WorkbenchLayoutComponent>;

  beforeEach(
    waitForAsync(() => {
      TestBed.configureTestingModule({
        providers: [ɵMockMatchMediaProvider],
        imports: [ComponentsModule, TestingModule]
      }).compileComponents();
    })
  );

  beforeEach(() => {
    fixture = TestBed.createComponent(WorkbenchLayoutComponent);
    component = fixture.componentInstance;
    fixture.detectChanges();
  });

  it('should create', () => {
    expect(component).toBeTruthy();
  });
});
